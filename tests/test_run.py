"""Hermetic positive and refusal tests for the shipping Crash launcher."""

from __future__ import annotations

import io
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from collections.abc import Sequence
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import run

LOCKED_PYTHON = "/locked/venv/bin/python"


class FakeHost(run.Host):
    def __init__(
        self,
        *,
        missing: set[str] | None = None,
        fail_token: str | None = None,
        system: str = "Linux",
        distribution: str = "fedora",
    ) -> None:
        self.missing = missing or set()
        self.fail_token = fail_token
        self.system_name = system
        self.distribution = distribution
        self.commands: list[tuple[list[str], dict[str, object]]] = []

    def which(self, name: str) -> str | None:
        if name in self.missing:
            return None
        return f"/fake/{name}"

    def system(self) -> str:
        return self.system_name

    def linux_distribution(self) -> str:
        return self.distribution

    def run(
        self, args: Sequence[str], **kwargs: object
    ) -> subprocess.CompletedProcess[str]:
        command = [str(item) for item in args]
        self.commands.append((command, kwargs))
        returncode = 1 if self.fail_token and self.fail_token in command else 0
        stdout = ""
        if command[:2] == ["getconf", "_NPROCESSORS_ONLN"]:
            stdout = "12"
        elif "rev-parse" in command:
            stdout = "abcdef12"
        return subprocess.CompletedProcess(command, returncode, stdout=stdout, stderr="")


class LauncherTests(unittest.TestCase):
    def setUp(self) -> None:
        (ROOT / "scratch/raw").mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(
            prefix="crash-run-test-", dir=ROOT / "scratch/raw"
        )
        self.root = Path(self.temporary.name)
        (self.root / "external/psxport/cmake").mkdir(parents=True)
        (self.root / "external/psxport/cmake/psxport.cmake").write_text(
            "# fixture\n", encoding="utf-8"
        )
        fixture_policy = self.root / "external/psxport/tools/port/launch_environment.py"
        fixture_policy.parent.mkdir(parents=True)
        shutil.copyfile(
            ROOT / "external/psxport/tools/port/launch_environment.py", fixture_policy
        )
        for relative in ("project.py", "automation/__init__.py", "automation/process.py"):
            target = self.root / "external/psxport/tools" / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT / "external/psxport/tools" / relative, target)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def invoke(
        self,
        host: FakeHost,
        *argv: str,
        environment: dict[str, str] | None = None,
    ) -> tuple[int, str, str]:
        stdout = io.StringIO()
        stderr = io.StringIO()
        code = run.run_launcher(
            argv,
            environ=environment or {"PATH": os.environ.get("PATH", "")},
            host=host,
            root=self.root,
            python_executable=LOCKED_PYTHON,
            stdout=stdout,
            stderr=stderr,
        )
        return code, stdout.getvalue(), stderr.getvalue()

    @staticmethod
    def commands(host: FakeHost) -> list[list[str]]:
        return [command for command, _ in host.commands]

    def test_default_provisions_builds_and_launches_dynarec_product(self) -> None:
        host = FakeHost()
        code, stdout, stderr = self.invoke(host, "Crash Bandicoot.chd")
        commands = self.commands(host)

        self.assertEqual(code, 0)
        self.assertEqual(stderr, "")
        self.assertIn("launching Crash Bandicoot 1 through Lightrec", stdout)
        self.assertIn([LOCKED_PYTHON, "tools/psxport_fetch.py", "--auto"], commands)
        provision = next(command for command in commands if "tools/provision_title.py" in command)
        self.assertEqual(provision[-1], "Crash Bandicoot.chd")
        self.assertIn(LOCKED_PYTHON, provision)
        self.assertFalse(any("recomp" in token or "generated" in token for command in commands for token in command))
        self.assertEqual(commands[-1], [str(self.root / run.PRODUCT)])
        self.assertFalse(any(command and command[0] == "ctest" for command in commands))
        forbidden_targets = {"crash_scaffold", "oracle_trace", "oracle_spike"}
        self.assertFalse(
            any(forbidden_targets.intersection(command) for command in commands),
            commands,
        )

        configure_commands = [
            command for command in commands if command[:2] == ["cmake", "-S"]
        ]
        self.assertEqual(len(configure_commands), 1)
        self.assertTrue(
            all("-DBUILD_TESTING=OFF" in command for command in configure_commands)
        )
        self.assertIn(f"-DPython3_EXECUTABLE={LOCKED_PYTHON}", configure_commands[0])
        self.assertEqual(configure_commands[0][5:7], ["-G", "Ninja"])
        self.assertIn("-DCMAKE_C_COMPILER=clang", configure_commands[0])
        self.assertIn("-DCMAKE_CXX_COMPILER=clang++", configure_commands[0])
        build_commands = [
            command for command in commands if command[:2] == ["cmake", "--build"]
        ]
        self.assertIn("discdump", build_commands[0])
        self.assertIn("crash1_port", build_commands[-1])

    def test_default_player_exec_strips_agent_policy_from_ambient_environment(self) -> None:
        host = FakeHost()
        poisoned = {
            "PATH": os.environ.get("PATH", ""),
            "PSXPORT_VK_HEADLESS": "1",
            "PSXPORT_NOAUDIO": "1",
            "PSXPORT_NOPACE": "1",
            "PSXPORT_NOWINDOW": "1",
            "KEEP_ME": "preserved",
        }

        code, _, stderr = self.invoke(host, environment=poisoned)
        launch_environment = host.commands[-1][1]["env"]

        self.assertEqual(code, 0)
        self.assertEqual(stderr, "")
        self.assertEqual(launch_environment["PSXPORT_VK_WINDOW"], "1")
        self.assertEqual(launch_environment["KEEP_ME"], "preserved")
        for agent_key in (
            "PSXPORT_VK_HEADLESS",
            "PSXPORT_NOAUDIO",
            "PSXPORT_NOPACE",
            "PSXPORT_NOWINDOW",
        ):
            self.assertNotIn(agent_key, launch_environment)

    def test_prepare_only_never_launches(self) -> None:
        host = FakeHost()
        code, stdout, stderr = self.invoke(host, "--prepare-only")

        self.assertEqual(code, 0)
        self.assertEqual(stderr, "")
        self.assertIn("native/Lightrec product is built", stdout)
        self.assertNotEqual(self.commands(host)[-1], [str(self.root / run.PRODUCT)])

    def test_runtime_dependency_overrides_reach_cmake(self) -> None:
        prefix = self.root / "build/lightning"
        for relative in ("include/lightning.h", "lib/liblightning.a"):
            target = prefix / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"fixture")
        lightrec = self.root / "build/lightrec"
        lightrec.mkdir(parents=True)
        for relative in ("CMakeLists.txt", "lightrec.h"):
            (lightrec / relative).write_text("fixture\n", encoding="utf-8")
        host = FakeHost()
        code, _, stderr = self.invoke(
            host,
            "--prepare-only",
            environment={"PSXPORT_LIGHTNING_PREFIX": str(prefix), "PSXPORT_LIGHTREC_DIR": str(lightrec)},
        )
        self.assertEqual((code, stderr), (0, ""))
        configure = next(command for command in self.commands(host) if command[:2] == ["cmake", "-S"])
        self.assertIn(f"-DPSXPORT_LIGHTREC_DIR={lightrec}", configure)
        self.assertIn(f"-DLIBLIGHTNING_INCLUDE_DIR={prefix / 'include'}", configure)
        self.assertIn(f"-DLIBLIGHTNING={prefix / 'lib/liblightning.a'}", configure)

    def test_incomplete_runtime_dependency_refuses_before_cmake(self) -> None:
        host = FakeHost()
        code, _, stderr = self.invoke(
            host, "--prepare-only", environment={"PSXPORT_LIGHTNING_PREFIX": str(self.root / "missing")}
        )
        self.assertEqual(code, 1)
        self.assertIn("PSXPORT_LIGHTNING_PREFIX is incomplete", stderr)
        self.assertFalse(any(command[:2] == ["cmake", "-S"] for command in self.commands(host)))

    def test_explicit_compilers_pass_through_without_identity_policy(self) -> None:
        host = FakeHost(missing={"clang", "clang++"})
        code, _, stderr = self.invoke(
            host,
            "--prepare-only",
            environment={"CC": "custom-c", "CXX": "custom-cxx"},
        )
        configure = next(
            command for command in self.commands(host) if command[:2] == ["cmake", "-S"]
        )

        self.assertEqual(code, 0)
        self.assertEqual(stderr, "")
        self.assertIn("-DCMAKE_C_COMPILER=custom-c", configure)
        self.assertIn("-DCMAKE_CXX_COMPILER=custom-cxx", configure)

    def test_cmake_discovers_compiler_when_clang_is_absent(self) -> None:
        host = FakeHost(missing={"clang", "clang++"})
        code, _, stderr = self.invoke(host, "--prepare-only")
        configure = next(
            command for command in self.commands(host) if command[:2] == ["cmake", "-S"]
        )

        self.assertEqual(code, 0)
        self.assertEqual(stderr, "")
        self.assertFalse(any("CMAKE_C_COMPILER=" in item for item in configure))
        self.assertFalse(any("CMAKE_CXX_COMPILER=" in item for item in configure))

    def test_missing_dependencies_name_exact_install_commands(self) -> None:
        cases = (
            (FakeHost(missing={"cmake"}), "sudo dnf install cmake"),
            (FakeHost(missing={"ninja"}), "sudo dnf install ninja-build"),
            (FakeHost(fail_token="sdl3", distribution="ubuntu"), "sudo apt install libsdl3-dev"),
            (FakeHost(missing={"cmake"}, system="Darwin"), "brew install cmake"),
        )
        for host, expected in cases:
            with self.subTest(expected=expected):
                code, _, stderr = self.invoke(host)
                self.assertEqual(code, 1)
                self.assertIn(expected, stderr)

    def test_provision_failure_stops_before_generation_and_build(self) -> None:
        host = FakeHost(fail_token="tools/provision_title.py")
        code, _, stderr = self.invoke(host)
        commands = self.commands(host)

        self.assertEqual(code, 1)
        self.assertIn("provisioning failed", stderr)
        self.assertFalse(any("tools/crash1_recomp.py" in command for command in commands))
        self.assertFalse(any("crash1_port" in command for command in commands))

    def test_shell_and_lock_are_the_stable_entry_contract(self) -> None:
        self.assertEqual(
            (ROOT / "run.sh").read_text(encoding="utf-8"),
            '#!/bin/sh\ncd "$(dirname "$0")" || exit 1\nexec uv run --frozen python bootstrap.py "$@"\n',
        )
        self.assertIn("from tools.run import main", (ROOT / "bootstrap.py").read_text())
        self.assertIn("package = false", (ROOT / "pyproject.toml").read_text())
        self.assertIn("version = 1", (ROOT / "uv.lock").read_text())
        self.assertTrue(os.access(ROOT / "run.sh", os.X_OK))

    def test_help_exits_before_host_or_disc_discovery(self) -> None:
        for flag in ("-h", "--help"):
            with self.subTest(flag=flag):
                result = subprocess.run(
                    [sys.executable, str(ROOT / "bootstrap.py"), flag],
                    cwd=self.root,
                    env={"PATH": ""},
                    check=False,
                    capture_output=True,
                    text=True,
                )

                self.assertEqual(result.returncode, 0)
                self.assertIn("usage:", result.stdout.lower())
                self.assertIn("Crash Bandicoot USA CHD", result.stdout)
                self.assertEqual(result.stderr, "")


if __name__ == "__main__":
    unittest.main()
