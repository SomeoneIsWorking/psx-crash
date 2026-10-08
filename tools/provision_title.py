#!/usr/bin/env python3
"""Shared disc resolver and verified boot-executable publisher for Crash titles."""

from __future__ import annotations

import argparse
import os
import pathlib
import re
import subprocess
import sys
import tempfile
import types
from collections.abc import Callable, Mapping
from dataclasses import dataclass

ROOT = pathlib.Path(__file__).resolve().parent.parent


class Refused(Exception):
    """The available input cannot support the requested title provisioning claim."""


@dataclass(frozen=True)
class ProvisionSpec:
    title: str
    slug: str
    env_keys: tuple[str, ...]

    @property
    def manifest(self) -> pathlib.Path:
        return ROOT / "titles" / self.slug / "executable.json"

    @property
    def output_dir(self) -> pathlib.Path:
        return ROOT / "scratch" / "bin" / self.slug


SPECS = {
    "crash1": ProvisionSpec(
        "Crash Bandicoot", "crash1", ("PSXPORT_CRASH1_DISC", "PSXPORT_DISC")
    ),
    "crash2": ProvisionSpec(
        "Crash Bandicoot 2: Cortex Strikes Back",
        "crash2",
        ("PSXPORT_CRASH2_DISC", "PSXPORT_DISC"),
    ),
    "crash3": ProvisionSpec(
        "Crash Bandicoot: Warped",
        "crash3",
        ("PSXPORT_CRASH3_DISC", "PSXPORT_DISC"),
    ),
}


@dataclass(frozen=True)
class ResolvedDisc:
    path: pathlib.Path
    source: str


def _path_from(value: str, root: pathlib.Path) -> pathlib.Path:
    path = pathlib.Path(value).expanduser()
    return path if path.is_absolute() else root / path


def _validate_disc(value: str, source: str, root: pathlib.Path) -> ResolvedDisc:
    path = _path_from(value, root)
    if not path.is_file():
        raise Refused(f"{source} names {path}, which is not a file")
    return ResolvedDisc(path.resolve(), source)


def _dotenv_values(path: pathlib.Path, env_keys: tuple[str, ...]) -> dict[str, str]:
    if not path.is_file():
        return {}
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        raise Refused(f"cannot read {path}: {exc}") from exc

    values: dict[str, str] = {}
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith("#") or "=" not in stripped:
            continue
        key, value = stripped.split("=", 1)
        key = key.strip()
        if key not in env_keys:
            continue
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        values[key] = value
    return values


def resolve_disc(
    spec: ProvisionSpec,
    argument: str | None,
    *,
    root: pathlib.Path = ROOT,
    environ: Mapping[str, str] = os.environ,
) -> ResolvedDisc:
    """Resolve one disc without falling through an invalid configured source."""
    if argument is not None:
        return _validate_disc(argument, "CLI argument", root)

    for key in spec.env_keys:
        if environ.get(key):
            return _validate_disc(environ[key], f"${key}", root)

    dotenv = _dotenv_values(root / ".env", spec.env_keys)
    for key in spec.env_keys:
        if dotenv.get(key):
            return _validate_disc(dotenv[key], f".env ({key})", root)

    dropins = sorted(root.glob("*.chd"), key=lambda item: item.name.casefold())
    if len(dropins) == 1:
        return _validate_disc(str(dropins[0]), "repository-root *.chd drop-in", root)
    if len(dropins) > 1:
        names = ", ".join(item.name for item in dropins)
        raise Refused(f"multiple repository-root CHDs are ambiguous: {names}")
    keys = " or ".join(spec.env_keys)
    raise Refused(
        f"no disc image; pass one as an argument, set {keys}, put either key in .env, "
        "or place one *.chd in the repository root"
    )


def find_discdump(
    override: str | None,
    *,
    root: pathlib.Path = ROOT,
    environ: Mapping[str, str] = os.environ,
) -> pathlib.Path:
    configured = override or environ.get("PSXPORT_DISCDUMP")
    if configured:
        path = _path_from(configured, root)
        if not path.is_file() or not os.access(path, os.X_OK):
            raise Refused(f"configured discdump is not executable: {path}")
        return path.resolve()

    build_dir = root / "build" / "verify"
    if not (build_dir / "CMakeCache.txt").is_file():
        raise Refused(
            "no configured Crash build; run the documented verifier or configure build/verify "
            "before provisioning"
        )

    result = subprocess.run(
        [
            "cmake",
            "--build",
            str(build_dir),
            "--target",
            "discdump",
            "-j",
            str(os.cpu_count() or 4),
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        raise Refused(f"could not build psxport discdump: {detail}")

    base = build_dir / "psxport_build" / "tools" / "discdump"
    for candidate in (base, base.with_suffix(".exe")):
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate.resolve()
    raise Refused(
        f"discdump target built but produced no executable below {base.parent}"
    )


def parse_boot_target(system_cnf: pathlib.Path) -> str:
    try:
        text = system_cnf.read_text(encoding="ascii", errors="replace")
    except OSError as exc:
        raise Refused(f"cannot read extracted {system_cnf.name}: {exc}") from exc
    match = re.search(
        r"^\s*BOOT\s*=\s*cdrom:\\+([^;\r\n]+)(?:;1)?\s*$",
        text,
        re.IGNORECASE | re.MULTILINE,
    )
    if not match:
        raise Refused("SYSTEM.CNF has no supported BOOT = cdrom:\\...;1 target")
    return pathlib.PureWindowsPath(match.group(1).strip()).name


def extract_boot(
    discdump: pathlib.Path, disc: pathlib.Path, output: pathlib.Path
) -> None:
    result = subprocess.run(
        [str(discdump), str(disc), str(output)],
        capture_output=True,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        raise Refused(f"discdump failed with exit {result.returncode}: {detail}")


def _identity_manifest(
    spec: ProvisionSpec,
) -> tuple[types.ModuleType, dict[str, object]]:
    sys.path.insert(0, str(ROOT / "tools"))
    import verify_executable

    return verify_executable, verify_executable.load_manifest(spec.manifest)


def expected_executable(spec: ProvisionSpec) -> str:
    _, manifest = _identity_manifest(spec)
    name = manifest.get("executable")
    if not isinstance(name, str) or not name:
        raise Refused("executable manifest has no usable executable name")
    return name


def verify_identity(spec: ProvisionSpec, executable: pathlib.Path) -> list[str]:
    verify_executable, manifest = _identity_manifest(spec)
    return verify_executable.check(manifest, executable)


def provision(
    spec: ProvisionSpec,
    disc: pathlib.Path,
    discdump: pathlib.Path,
    *,
    output_dir: pathlib.Path | None = None,
    extract: Callable[[pathlib.Path, pathlib.Path, pathlib.Path], None] = extract_boot,
    identity_check: Callable[[pathlib.Path], list[str]] | None = None,
) -> pathlib.Path:
    """Extract, validate, then publish the title boot executable and SYSTEM.CNF."""
    publish_dir = output_dir or spec.output_dir
    publish_dir.mkdir(parents=True, exist_ok=True)
    check_identity = identity_check or (lambda path: verify_identity(spec, path))
    with tempfile.TemporaryDirectory(
        prefix=f"{spec.slug}-provision-", dir=publish_dir.parent
    ) as temporary:
        staging = pathlib.Path(temporary)
        extract(discdump, disc, staging)
        system_cnf = staging / "SYSTEM.CNF"
        boot_target = parse_boot_target(system_cnf)
        executable_name = expected_executable(spec)
        if boot_target.casefold() != executable_name.casefold():
            raise Refused(
                f"SYSTEM.CNF boots {boot_target!r}, expected {executable_name!r}"
            )

        executable = staging / executable_name
        if not executable.is_file():
            raise Refused(f"discdump did not extract {executable_name}")
        failures = check_identity(executable)
        if failures:
            raise Refused(
                f"executable identity disagrees on {len(failures)} tracked fact(s)"
            )

        destination_exe = publish_dir / executable.name
        destination_cnf = publish_dir / system_cnf.name
        os.replace(executable, destination_exe)
        os.replace(system_cnf, destination_cnf)
    return destination_exe


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Provision one serial-identified Crash title's verified boot executable from CHD."
    )
    parser.add_argument(
        "--title", choices=sorted(SPECS), required=True, help="title integration"
    )
    parser.add_argument("disc", nargs="?", help="selected title's USA CHD")
    parser.add_argument("--discdump", help="executable psxport discdump override")
    parser.add_argument(
        "--resolve-only",
        action="store_true",
        help="print the selected disc without extracting",
    )
    args = parser.parse_args()
    spec = SPECS[args.title]

    try:
        resolved = resolve_disc(spec, args.disc)
        print(f"[disc] {resolved.source}: {resolved.path}", file=sys.stderr)
        if args.resolve_only:
            print(resolved.path)
            return 0
        discdump = find_discdump(args.discdump)
        executable = provision(spec, resolved.path, discdump)
        print("MATCH: SYSTEM.CNF boot target and executable identity agree")
        print(f"provisioned {executable.relative_to(ROOT)}")
        print(
            "blind spot: this proves the selected disc's boot executable identity, not boot, "
            "gameplay, overlays, or native implementation"
        )
        return 0
    except (OSError, Refused) as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
