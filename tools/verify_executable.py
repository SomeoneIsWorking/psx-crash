#!/usr/bin/env python3
"""Compare a title's tracked executable and VSync facts with a real PS-X EXE.

Exit 0 means every declared fact matched, exit 1 means the image contradicted the manifest, and exit
2 means no valid comparison was possible. This is an executable-facts gate, not a boot gate.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import sys
from dataclasses import dataclass

ROOT = pathlib.Path(__file__).resolve().parent.parent
PSXPORT = ROOT / "external" / "psxport"
sys.path.insert(0, str(PSXPORT))
try:
    from tools.formats import psx_exe
except ImportError as exc:
    raise SystemExit(
        f"REFUSED: cannot import psxport's PS-X EXE loader from {PSXPORT}; "
        "run tools/psxport_fetch.py --auto"
    ) from exc


class Refused(Exception):
    """The input cannot support the requested identity claim."""


@dataclass(frozen=True)
class Measurement:
    name: str
    size: int
    sha1: str
    sha256: str
    entry: int
    gp: int
    text_address: int
    text_size: int
    stack_address: int
    stack_offset: int
    markers: tuple[str, ...]
    vsync_body_sha256: str


@dataclass(frozen=True)
class BodyRange:
    path: str
    begin: int
    end: int
    body_sha256: str


@dataclass(frozen=True)
class AddressLoad:
    path: str
    entry: int
    address: int


def parse_hex(value: object, field: str) -> int:
    if not isinstance(value, str):
        raise Refused(f"manifest field {field} must be a hexadecimal string")
    try:
        return int(value, 16)
    except ValueError as exc:
        raise Refused(f"manifest field {field} is not hexadecimal: {value!r}") from exc


def load_manifest(path: pathlib.Path) -> dict[str, object]:
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise Refused(f"cannot read manifest {path}: {exc}") from exc
    required = {
        "title",
        "region",
        "serial",
        "executable",
        "file_size",
        "sha1",
        "sha256",
        "header",
        "region_markers",
    }
    missing = sorted(required - manifest.keys())
    if missing:
        raise Refused(f"manifest {path} is missing {', '.join(missing)}")
    return manifest


def vsync_range(manifest: dict[str, object]) -> tuple[int, int, str]:
    runtime = manifest.get("runtime")
    if not isinstance(runtime, dict):
        raise Refused("manifest field runtime must be an object")
    vsync = runtime.get("vsync")
    if not isinstance(vsync, dict):
        raise Refused("manifest field runtime.vsync must be an object")
    begin = parse_hex(vsync.get("entry"), "runtime.vsync.entry")
    end = parse_hex(vsync.get("end"), "runtime.vsync.end")
    body_sha256 = vsync.get("body_sha256")
    if begin == 0 or begin >= end:
        raise Refused(f"invalid runtime.vsync range [0x{begin:08X},0x{end:08X})")
    if not isinstance(body_sha256, str) or len(body_sha256) != 64:
        raise Refused(
            "manifest field runtime.vsync.body_sha256 must be a SHA-256 string"
        )
    try:
        int(body_sha256, 16)
    except ValueError as exc:
        raise Refused(
            "manifest field runtime.vsync.body_sha256 is not hexadecimal"
        ) from exc
    return begin, end, body_sha256


def runtime_body_ranges(manifest: dict[str, object]) -> tuple[BodyRange, ...]:
    runtime = manifest.get("runtime")
    if not isinstance(runtime, dict):
        raise Refused("manifest field runtime must be an object")
    found: list[BodyRange] = []

    def visit(value: object, path: str) -> None:
        if not isinstance(value, dict):
            return
        if {"entry", "end", "body_sha256"} <= value.keys():
            if path == "runtime.vsync":
                return
            begin = parse_hex(value.get("entry"), f"{path}.entry")
            end = parse_hex(value.get("end"), f"{path}.end")
            digest = value.get("body_sha256")
            if begin == 0 or begin >= end:
                raise Refused(f"invalid {path} range [0x{begin:08X},0x{end:08X})")
            if not isinstance(digest, str) or len(digest) != 64:
                raise Refused(
                    f"manifest field {path}.body_sha256 must be a SHA-256 string"
                )
            try:
                int(digest, 16)
            except ValueError as exc:
                raise Refused(
                    f"manifest field {path}.body_sha256 is not hexadecimal"
                ) from exc
            found.append(BodyRange(path, begin, end, digest))
            return
        for key, child in value.items():
            visit(child, f"{path}.{key}")

    visit(runtime, "runtime")
    return tuple(sorted(found, key=lambda item: item.path))


def runtime_address_loads(manifest: dict[str, object]) -> tuple[AddressLoad, ...]:
    runtime = manifest.get("runtime")
    if not isinstance(runtime, dict):
        raise Refused("manifest field runtime must be an object")
    found: list[AddressLoad] = []

    def visit(value: object, path: str) -> None:
        if not isinstance(value, dict):
            return
        if {"address", "load_entry"} <= value.keys():
            found.append(
                AddressLoad(
                    path,
                    parse_hex(value.get("load_entry"), f"{path}.load_entry"),
                    parse_hex(value.get("address"), f"{path}.address"),
                )
            )
            return
        for key, child in value.items():
            visit(child, f"{path}.{key}")

    visit(runtime, "runtime")
    return tuple(sorted(found, key=lambda item: item.path))


def measure_runtime_address_loads(
    path: pathlib.Path, manifest: dict[str, object]
) -> dict[str, int]:
    try:
        data = path.read_bytes()
        image = psx_exe.load(str(path))
    except (OSError, ValueError) as exc:
        raise Refused(f"cannot read a valid executable from {path}: {exc}") from exc
    measured: dict[str, int] = {}
    for load in runtime_address_loads(manifest):
        if not (image.load <= load.entry and load.entry + 8 <= image.text_end):
            raise Refused(
                f"{load.path}.load_entry 0x{load.entry:08X} lies outside executable text "
                f"[0x{image.load:08X},0x{image.text_end:08X})"
            )
        file_begin = 0x800 + (load.entry - image.load)
        if file_begin + 8 > len(data):
            raise Refused(f"{load.path}.load_entry is not backed by executable file bytes")
        lui = int.from_bytes(data[file_begin : file_begin + 4], "little")
        load_word = int.from_bytes(data[file_begin + 4 : file_begin + 8], "little")
        register = (lui >> 16) & 0x1F
        if lui >> 26 != 0x0F or load_word >> 26 != 0x23:
            raise Refused(f"{load.path}.load_entry is not a LUI/LW address load")
        if ((load_word >> 21) & 0x1F) != register or ((load_word >> 16) & 0x1F) != register:
            raise Refused(f"{load.path}.load_entry LUI/LW does not use one register")
        lower = load_word & 0xFFFF
        signed_lower = lower if lower < 0x8000 else lower - 0x10000
        measured[load.path] = (((lui & 0xFFFF) << 16) + signed_lower) & 0xFFFFFFFF
    return measured


def measure_runtime_bodies(
    path: pathlib.Path, manifest: dict[str, object]
) -> dict[str, str]:
    try:
        data = path.read_bytes()
        image = psx_exe.load(str(path))
    except (OSError, ValueError) as exc:
        raise Refused(f"cannot read a valid executable from {path}: {exc}") from exc
    measured: dict[str, str] = {}
    for body in runtime_body_ranges(manifest):
        if not image.load <= body.begin < body.end <= image.text_end:
            raise Refused(
                f"{body.path} [0x{body.begin:08X},0x{body.end:08X}) lies outside "
                f"executable text [0x{image.load:08X},0x{image.text_end:08X})"
            )
        file_begin = 0x800 + (body.begin - image.load)
        file_end = 0x800 + (body.end - image.load)
        if file_end > len(data):
            raise Refused(f"{body.path} range is not backed by executable file bytes")
        measured[body.path] = hashlib.sha256(data[file_begin:file_end]).hexdigest()
    return measured


def measure(
    path: pathlib.Path, markers: list[str], vsync_begin: int, vsync_end: int
) -> Measurement:
    try:
        data = path.read_bytes()
        image = psx_exe.load(str(path))
    except (OSError, ValueError) as exc:
        raise Refused(f"cannot read a valid executable from {path}: {exc}") from exc
    if image.text_size == 0 or not image.load <= image.entry < image.text_end:
        raise Refused(
            f"invalid PS-X EXE range: entry=0x{image.entry:08X}, "
            f"text=[0x{image.load:08X},0x{image.text_end:08X})"
        )
    if not image.load <= vsync_begin < vsync_end <= image.text_end:
        raise Refused(
            f"runtime.vsync [0x{vsync_begin:08X},0x{vsync_end:08X}) lies outside "
            f"executable text [0x{image.load:08X},0x{image.text_end:08X})"
        )
    vsync_file_begin = 0x800 + (vsync_begin - image.load)
    vsync_file_end = 0x800 + (vsync_end - image.load)
    if vsync_file_end > len(data):
        raise Refused("runtime.vsync range is not backed by executable file bytes")
    found = tuple(marker for marker in markers if marker.encode("ascii") in data)
    return Measurement(
        path.name,
        len(data),
        hashlib.sha1(data).hexdigest(),
        hashlib.sha256(data).hexdigest(),
        image.entry,
        image.gp,
        image.load,
        image.text_size,
        image.sp_base,
        image.sp_off,
        found,
        hashlib.sha256(data[vsync_file_begin:vsync_file_end]).hexdigest(),
    )


def measure_manifest(path: pathlib.Path, manifest: dict[str, object]) -> Measurement:
    markers = manifest.get("region_markers")
    if not isinstance(markers, list) or not all(
        isinstance(marker, str) for marker in markers
    ):
        raise Refused("manifest field region_markers must be a string list")
    vsync_begin, vsync_end, _ = vsync_range(manifest)
    return measure(path, markers, vsync_begin, vsync_end)


def expected(manifest: dict[str, object]) -> dict[str, object]:
    header = manifest["header"]
    markers = manifest["region_markers"]
    if not isinstance(header, dict):
        raise Refused("manifest field header must be an object")
    if (
        not isinstance(markers, list)
        or not markers
        or not all(isinstance(item, str) for item in markers)
    ):
        raise Refused("manifest field region_markers must be a non-empty string list")
    _, _, vsync_body_sha256 = vsync_range(manifest)
    return {
        "name": manifest["executable"],
        "size": manifest["file_size"],
        "sha1": manifest["sha1"],
        "sha256": manifest["sha256"],
        "entry": parse_hex(header.get("entry"), "header.entry"),
        "gp": parse_hex(header.get("gp"), "header.gp"),
        "text_address": parse_hex(header.get("text_address"), "header.text_address"),
        "text_size": parse_hex(header.get("text_size"), "header.text_size"),
        "stack_address": parse_hex(header.get("stack_address"), "header.stack_address"),
        "stack_offset": parse_hex(header.get("stack_offset"), "header.stack_offset"),
        "markers": tuple(markers),
        "vsync_body_sha256": vsync_body_sha256,
    }


def check(
    manifest: dict[str, object], path: pathlib.Path, verbose: bool = True
) -> list[str]:
    markers = manifest["region_markers"]
    if not isinstance(markers, list):
        raise Refused("manifest field region_markers must be a list")
    vsync_begin, vsync_end, _ = vsync_range(manifest)
    measured = measure_manifest(path, manifest)
    want = expected(manifest)
    actual = vars(measured)
    failures = [
        f"{field}: manifest={value!r}, executable={actual[field]!r}"
        for field, value in want.items()
        if value != actual[field]
    ]
    runtime_bodies = runtime_body_ranges(manifest)
    measured_bodies = measure_runtime_bodies(path, manifest)
    failures.extend(
        f"runtime_body_sha256:{body.path}: manifest={body.body_sha256!r}, "
        f"executable={measured_bodies[body.path]!r}"
        for body in runtime_bodies
        if body.body_sha256 != measured_bodies[body.path]
    )
    address_loads = runtime_address_loads(manifest)
    measured_address_loads = measure_runtime_address_loads(path, manifest)
    failures.extend(
        f"runtime_address_load:{load.path}: manifest=0x{load.address:08X}, "
        f"executable=0x{measured_address_loads[load.path]:08X}"
        for load in address_loads
        if load.address != measured_address_loads[load.path]
    )
    if verbose:
        print(
            f"measured {measured.name}: {measured.size} bytes, sha1={measured.sha1}, "
            f"sha256={measured.sha256}"
        )
        print(
            f"header: entry=0x{measured.entry:08X}, gp=0x{measured.gp:08X}, "
            f"text=[0x{measured.text_address:08X},"
            f"0x{measured.text_address + measured.text_size:08X}) ({measured.text_size} bytes), "
            f"stack=0x{measured.stack_address + measured.stack_offset:08X}"
        )
        print(f"region markers: {len(measured.markers)}/{len(markers)} found")
        print(
            f"VSync: [0x{vsync_begin:08X},0x{vsync_end:08X}) "
            f"body_sha256={measured.vsync_body_sha256}"
        )
        for body in runtime_bodies:
            print(
                f"{body.path}: [0x{body.begin:08X},0x{body.end:08X}) "
                f"body_sha256={measured_bodies[body.path]}"
            )
        for load in address_loads:
            print(
                f"{load.path}: load_entry=0x{load.entry:08X} "
                f"address=0x{measured_address_loads[load.path]:08X}"
            )
        if failures:
            for failure in failures:
                print(f"MISMATCH: {failure}")
        else:
            print(
                f"MATCH: {12 + len(runtime_bodies) + len(address_loads)}/"
                f"{12 + len(runtime_bodies) + len(address_loads)} facts agree "
                f"for {manifest['title']} ({manifest['region']})"
            )
        print(
            "blind spot: executable facts only; this does not prove disc provenance, boot, gameplay, "
            "VSync callers, overlays, or any native implementation"
        )
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--title", default="crash1", help="directory below titles/ (default: crash1)"
    )
    parser.add_argument("--exe", type=pathlib.Path, help="path to the real executable")
    parser.add_argument("--check", action="store_true", help="compare executable to manifest")
    args = parser.parse_args()

    manifest_path = ROOT / "titles" / args.title / "executable.json"
    try:
        manifest = load_manifest(manifest_path)
        executable = args.exe or ROOT / "scratch" / "bin" / args.title / str(
            manifest["executable"]
        )
        return 1 if check(manifest, executable) else 0
    except (OSError, Refused) as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
