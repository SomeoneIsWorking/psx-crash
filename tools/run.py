#!/usr/bin/env python3
"""Provision, build, and launch the Crash 1 native/Lightrec product."""

from __future__ import annotations

import argparse
import os
import platform
import runpy
import shutil
import subprocess
import sys
from collections.abc import Mapping, Sequence
from pathlib import Path
from typing import TextIO

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path("build/player")
EXECUTABLE = Path("scratch/bin/crash1/SCUS_949.00")
PRODUCT = BUILD / "crash1_port"


class LauncherFailure(RuntimeError):
    """A user-facing launcher refusal."""


class Host:
    """Injectable host discovery and execution seam for hermetic launcher tests."""

    @staticmethod
    def which(name: str) -> str | None:
        return shutil.which(name)

    @staticmethod
    def run(args: Sequence[str], **kwargs: object) -> subprocess.CompletedProcess:
        check = bool(kwargs.pop("check", False))
        return subprocess.run(list(args), check=check, **kwargs)

    @staticmethod
    def system() -> str:
        return platform.system()

    @staticmethod
    def linux_distribution() -> str:
        try:
            for line in Path("/etc/os-release").read_text(encoding="utf-8").splitlines():
                key, separator, value = line.partition("=")
                if separator and key == "ID":
                    return value.strip().strip('"').lower()
        except OSError:
            pass
        return "unknown"


def package_command(host: Host, package: str) -> str | None:
    system = host.system()
    if system == "Darwin":
        return {
            "cmake": "brew install cmake",
            "git": "xcode-select --install",
            "ninja": "brew install ninja",
            "pkg-config": "brew install pkg-config",
            "sdl3": "brew install sdl3",
        }[package]
    if system == "Windows":
        return {
            "cmake": "winget install Kitware.CMake",
            "git": "winget install Git.Git",
            "ninja": "winget install Ninja-build.Ninja",
            "pkg-config": "vcpkg install pkgconf",
            "sdl3": "vcpkg install sdl3",
        }[package]
    if system != "Linux":
        return None
    distribution = host.linux_distribution()
    if distribution in {"fedora", "rhel", "centos", "rocky", "almalinux"}:
        return {
            "cmake": "sudo dnf install cmake",
            "git": "sudo dnf install git",
            "ninja": "sudo dnf install ninja-build",
            "pkg-config": "sudo dnf install pkgconf-pkg-config",
            "sdl3": "sudo dnf install SDL3-devel",
        }[package]
    if distribution in {"debian", "ubuntu", "linuxmint", "pop"}:
        return {
            "cmake": "sudo apt install cmake",
            "git": "sudo apt install git",
            "ninja": "sudo apt install ninja-build",
            "pkg-config": "sudo apt install pkg-config",
            "sdl3": "sudo apt install libsdl3-dev",
        }[package]
    return None


def missing_dependency(host: Host, name: str, package: str) -> LauncherFailure:
    command = package_command(host, package)
    if command:
        return LauncherFailure(f"{name} not found. Install it with: {command}")
    system = host.system()
    distribution = host.linux_distribution() if system == "Linux" else "unknown"
    return LauncherFailure(
        f"{name} not found, and no package command is recorded for {system}/{distribution}; "
        "install it with your platform package manager and rerun"
    )


def require_tool(host: Host, name: str, package: str | None = None) -> str:
    resolved = host.which(name)
    if resolved is None:
        raise missing_dependency(host, name, package or name)
    return resolved


def run_stage(
    host: Host,
    args: Sequence[str],
    failure: str,
    *,
    root: Path,
    environment: Mapping[str, str],
) -> None:
    try:
        result = host.run(args, cwd=root, env=dict(environment), check=False)
    except OSError as exc:
        raise LauncherFailure(f"{failure}: {exc}") from exc
    if result.returncode != 0:
        raise LauncherFailure(failure)


def command_output(host: Host, args: Sequence[str], *, root: Path) -> tuple[int, str]:
    try:
        result = host.run(
            args,
            cwd=root,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
    except OSError:
        return 127, ""
    return result.returncode, result.stdout.strip()


def compiler_arguments(host: Host, environment: Mapping[str, str]) -> list[str]:
    """Pass caller selections through; otherwise prefer Clang and leave discovery to CMake."""
    arguments = []
    if cc := environment.get("CC"):
        arguments.append(f"-DCMAKE_C_COMPILER={cc}")
    if cxx := environment.get("CXX"):
        arguments.append(f"-DCMAKE_CXX_COMPILER={cxx}")
    if arguments:
        return arguments
    if host.which("clang") is not None and host.which("clang++") is not None:
        return ["-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++"]
    return []


def cpu_jobs(host: Host, *, root: Path) -> str:
    for command in (["getconf", "_NPROCESSORS_ONLN"], ["sysctl", "-n", "hw.ncpu"]):
        returncode, output = command_output(host, command, root=root)
        if returncode == 0 and output.isdigit() and int(output) > 0:
            return output
    return "4"


def framework_revision(host: Host, path: Path, *, root: Path) -> tuple[str, bool]:
    returncode, revision = command_output(
        host, ["git", "-C", str(path), "rev-parse", "--short", "HEAD"], root=root
    )
    if returncode != 0 or not revision:
        revision = "?"
    _, status = command_output(
        host, ["git", "-C", str(path), "status", "--porcelain"], root=root
    )
    return revision, bool(status)


def configure_command(
    python_executable: str, framework: Path, compiler_options: Sequence[str]
) -> list[str]:
    return [
        "cmake",
        "-S",
        ".",
        "-B",
        str(BUILD),
        "-G",
        "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DBUILD_TESTING=OFF",
        f"-DPSXPORT_DIR={framework}",
        f"-DPython3_EXECUTABLE={python_executable}",
        *compiler_options,
    ]


def runtime_build_arguments(framework: Path, environment: Mapping[str, str]) -> list[str]:
    """Use the framework's dependency resolution for both player and verifier builds."""
    project = framework / "tools/project.py"
    if not project.is_file():
        raise LauncherFailure(f"psxport checkout is missing build policy: {project}")
    sys.path.insert(0, str(project.parent))
    try:
        policy = runpy.run_path(str(project))
    finally:
        sys.path.pop(0)
    try:
        return [
            *policy["lightrec_cmake_definitions"](environment),
            *policy["lightning_cmake_definitions"](environment),
        ]
    except policy["ToolError"] as exc:
        raise LauncherFailure(str(exc)) from exc


def player_launch_environment(
    framework: Path, environment: Mapping[str, str]
) -> dict[str, str]:
    """Apply psxport's shared policy at the final product-exec boundary."""
    policy_path = framework / "tools/port/launch_environment.py"
    if not policy_path.is_file():
        raise LauncherFailure(
            f"psxport checkout is missing shipping launch policy: {policy_path}"
        )
    policy = runpy.run_path(str(policy_path))
    apply_policy = policy.get("player_environment")
    if not callable(apply_policy):
        raise LauncherFailure(f"invalid psxport shipping launch policy: {policy_path}")
    # `product` names this title's run-log directory and psxport requires it.
    return apply_policy(environment, product="crash")


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("disc", nargs="?", help="path to the user's Crash Bandicoot USA CHD")
    parser.add_argument(
        "--prepare-only",
        action="store_true",
        help="provision and build the current Crash 1 product without launching it",
    )
    return parser.parse_args(list(argv))


def run_launcher(
    argv: Sequence[str],
    *,
    environ: Mapping[str, str] | None = None,
    host: Host | None = None,
    root: Path = ROOT,
    python_executable: str = sys.executable,
    stdout: TextIO = sys.stdout,
    stderr: TextIO = sys.stderr,
) -> int:
    environment = dict(os.environ if environ is None else environ)
    machine = host or Host()
    try:
        options = parse_args(argv)
        require_tool(machine, "cmake")
        require_tool(machine, "git")
        require_tool(machine, "ninja")
        require_tool(machine, "pkg-config")
        run_stage(
            machine,
            ["pkg-config", "--exists", "sdl3"],
            str(missing_dependency(machine, "SDL3 development files", "sdl3")),
            root=root,
            environment=environment,
        )

        framework_setting = environment.get("PSXPORT_DIR") or "external/psxport"
        if "PSXPORT_DIR" not in environment:
            run_stage(
                machine,
                [python_executable, "tools/psxport_fetch.py", "--auto"],
                "could not resolve external/psxport",
                root=root,
                environment=environment,
            )
        framework = Path(framework_setting)
        if not framework.is_absolute():
            framework = root / framework
        if not (framework / "cmake" / "psxport.cmake").is_file():
            raise LauncherFailure(f"PSXPORT_DIR={framework_setting} is not a psxport checkout")
        revision, dirty = framework_revision(machine, framework, root=root)
        suffix = " +dirty" if dirty else ""
        print(f"[run] framework: {framework.resolve()} @ {revision}{suffix}", file=stdout)

        compiler_options = compiler_arguments(machine, environment)
        jobs = cpu_jobs(machine, root=root)
        configure = configure_command(python_executable, framework, compiler_options)
        configure.extend(runtime_build_arguments(framework, environment))
        run_stage(
            machine,
            configure,
            "initial CMake configure failed",
            root=root,
            environment=environment,
        )
        run_stage(
            machine,
            ["cmake", "--build", str(BUILD), "--parallel", jobs, "--target", "discdump"],
            "psxport disc reader build failed",
            root=root,
            environment=environment,
        )

        provision_environment = dict(environment)
        provision_environment["PSXPORT_DIR"] = str(framework)
        provision = [
            python_executable,
            "tools/provision_title.py",
            "--title",
            "crash1",
            "--discdump",
            str(BUILD / "psxport_build/tools/discdump"),
        ]
        if options.disc:
            provision.append(options.disc)
        run_stage(
            machine,
            provision,
            "Crash 1 executable provisioning failed",
            root=root,
            environment=provision_environment,
        )
        run_stage(
            machine,
            ["cmake", "--build", str(BUILD), "--parallel", jobs, "--target", "crash1_port"],
            "Crash 1 product build failed",
            root=root,
            environment=environment,
        )
    except LauncherFailure as exc:
        print(f"[run] error: {exc}", file=stderr)
        return 1

    if options.prepare_only:
        print("[run] Crash 1 native/Lightrec product is built.", file=stdout)
        return 0

    print("[run] launching Crash Bandicoot 1 through Lightrec…", file=stdout)
    try:
        launch_environment = player_launch_environment(framework, environment)
        result = machine.run(
            [str(root / PRODUCT)], cwd=root, env=launch_environment, check=False
        )
    except LauncherFailure as exc:
        print(f"[run] error: {exc}", file=stderr)
        return 1
    except OSError as exc:
        print(f"[run] error: launch failed: {exc}", file=stderr)
        return 1
    return result.returncode


def main(argv: Sequence[str] | None = None) -> int:
    return run_launcher(sys.argv[1:] if argv is None else argv)


if __name__ == "__main__":
    raise SystemExit(main())
