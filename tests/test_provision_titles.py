"""Both-answer tests for serial-separated Crash title provisioning."""

from __future__ import annotations

import pathlib
import stat
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import provision_title

CRASH1 = provision_title.SPECS["crash1"]
CRASH2 = provision_title.SPECS["crash2"]
CRASH3 = provision_title.SPECS["crash3"]
SPECS = (CRASH1, CRASH2, CRASH3)


class ResolverTests(unittest.TestCase):
    def setUp(self) -> None:
        (ROOT / "scratch").mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=ROOT / "scratch")
        self.root = pathlib.Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def disc(self, name: str) -> pathlib.Path:
        path = self.root / name
        path.write_bytes(b"test CHD placeholder")
        return path

    def test_cli_wins_over_environment_and_dotenv_for_each_title(self) -> None:
        for spec in SPECS:
            with self.subTest(title=spec.slug):
                cli = self.disc(f"{spec.slug}-cli.chd")
                env = self.disc(f"{spec.slug}-env.chd")
                dot = self.disc(f"{spec.slug}-dot.chd")
                (self.root / ".env").write_text(
                    f"{spec.env_keys[0]}={dot}\n", encoding="utf-8"
                )
                resolved = provision_title.resolve_disc(
                    spec, str(cli), root=self.root, environ={spec.env_keys[0]: str(env)}
                )
                self.assertEqual(resolved.path, cli.resolve())
                self.assertEqual(resolved.source, "CLI argument")

    def test_invalid_configured_path_refuses_instead_of_falling_through(self) -> None:
        self.disc("dropin.chd")
        for spec in SPECS:
            with (
                self.subTest(title=spec.slug),
                self.assertRaisesRegex(provision_title.Refused, "not a file"),
            ):
                provision_title.resolve_disc(
                    spec,
                    None,
                    root=self.root,
                    environ={spec.env_keys[0]: "missing.chd"},
                )

    def test_dotenv_precedes_single_dropin_and_resolves_relative_to_repo(self) -> None:
        selected = self.disc("configured.chd")
        self.disc("dropin.chd")
        for spec in SPECS:
            with self.subTest(title=spec.slug):
                (self.root / ".env").write_text(
                    f"{spec.env_keys[0]}='configured.chd'\n", encoding="utf-8"
                )
                resolved = provision_title.resolve_disc(
                    spec, None, root=self.root, environ={}
                )
                self.assertEqual(resolved.path, selected.resolve())

    def test_generic_environment_and_dotenv_are_supported(self) -> None:
        selected = self.disc("generic.chd")
        for spec in SPECS:
            with self.subTest(title=spec.slug, source="environment"):
                resolved = provision_title.resolve_disc(
                    spec, None, root=self.root, environ={"PSXPORT_DISC": selected.name}
                )
                self.assertEqual(resolved.path, selected.resolve())
            with self.subTest(title=spec.slug, source="dotenv"):
                (self.root / ".env").write_text(
                    f"PSXPORT_DISC={selected.name}\n", encoding="utf-8"
                )
                resolved = provision_title.resolve_disc(
                    spec, None, root=self.root, environ={}
                )
                self.assertEqual(resolved.path, selected.resolve())

    def test_title_specific_keys_do_not_cross_select(self) -> None:
        selected = self.disc("other-title.chd")
        for requested in SPECS:
            for other in SPECS:
                if requested is other:
                    continue
                with (
                    self.subTest(requested=requested.slug, configured=other.slug),
                    self.assertRaisesRegex(provision_title.Refused, "no disc image"),
                ):
                    provision_title.resolve_disc(
                        requested,
                        None,
                        root=self.root / "empty",
                        environ={other.env_keys[0]: str(selected)},
                    )

    def test_single_dropin_is_supported_and_multiple_are_refused(self) -> None:
        dropin_root = self.root / "dropins"
        dropin_root.mkdir()
        selected = dropin_root / "one.chd"
        selected.write_bytes(b"one")
        for spec in SPECS:
            with self.subTest(title=spec.slug):
                resolved = provision_title.resolve_disc(
                    spec, None, root=dropin_root, environ={}
                )
                self.assertEqual(resolved.path, selected.resolve())
        (dropin_root / "two.chd").write_bytes(b"two")
        for spec in SPECS:
            with (
                self.subTest(title=spec.slug),
                self.assertRaisesRegex(provision_title.Refused, "ambiguous"),
            ):
                provision_title.resolve_disc(spec, None, root=dropin_root, environ={})


class ProvisionTests(unittest.TestCase):
    def setUp(self) -> None:
        (ROOT / "scratch").mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=ROOT / "scratch")
        self.root = pathlib.Path(self.temporary.name)
        self.disc = self.root / "disc.chd"
        self.disc.write_bytes(b"test CHD placeholder")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def discdump(
        self, boot_target: str, *, extra_executables: tuple[str, ...] = ()
    ) -> pathlib.Path:
        tool = self.root / f"fake-{boot_target.replace('/', '-')}.py"
        tool.write_text(
            "#!/usr/bin/env python3\n"
            "import pathlib, sys\n"
            "out = pathlib.Path(sys.argv[2])\n"
            "out.mkdir(parents=True, exist_ok=True)\n"
            f"(out / 'SYSTEM.CNF').write_text('BOOT = cdrom:\\\\{boot_target};1\\r\\n')\n"
            f"boot = out / {boot_target!r}\n"
            "boot.parent.mkdir(parents=True, exist_ok=True)\n"
            "boot.write_bytes(b'identity fixture')\n"
            f"extras = {extra_executables!r}\n"
            "for name in extras:\n"
            "    extra = out / name\n"
            "    extra.parent.mkdir(parents=True, exist_ok=True)\n"
            "    extra.write_bytes(b'extra executable fixture')\n",
            encoding="utf-8",
        )
        tool.chmod(tool.stat().st_mode | stat.S_IXUSR)
        return tool

    def test_matching_boot_and_identity_publish_for_each_title(self) -> None:
        for spec in SPECS:
            with self.subTest(title=spec.slug):
                executable = provision_title.expected_executable(spec)
                output = self.root / f"{spec.slug}-out"
                extras = ("DRAGON/SPYRO.EXE",) if spec is CRASH3 else ()
                result = provision_title.provision(
                    spec,
                    self.disc,
                    self.discdump(executable, extra_executables=extras),
                    output_dir=output,
                    identity_check=lambda _: [],
                )
                self.assertEqual(result.read_bytes(), b"identity fixture")
                self.assertTrue((output / "SYSTEM.CNF").is_file())
                self.assertFalse((output / "DRAGON" / "SPYRO.EXE").exists())

    def test_identity_disagreement_refuses_without_publishing(self) -> None:
        for spec in SPECS:
            with self.subTest(title=spec.slug):
                executable = provision_title.expected_executable(spec)
                output = self.root / f"{spec.slug}-identity-fail"
                with self.assertRaisesRegex(provision_title.Refused, "1 tracked fact"):
                    provision_title.provision(
                        spec,
                        self.disc,
                        self.discdump(executable),
                        output_dir=output,
                        identity_check=lambda _: ["sha256 mismatch"],
                    )
                self.assertFalse((output / executable).exists())
                self.assertFalse((output / "SYSTEM.CNF").exists())

    def test_other_crash_title_boot_target_refuses_without_publishing(self) -> None:
        for requested, other in ((CRASH1, CRASH2), (CRASH2, CRASH3), (CRASH3, CRASH1)):
            expected = provision_title.expected_executable(requested)
            wrong = provision_title.expected_executable(other)
            output = self.root / f"wrong-{requested.slug}"
            with (
                self.subTest(requested=requested.slug, booted=other.slug),
                self.assertRaisesRegex(provision_title.Refused, f"{wrong}.*{expected}"),
            ):
                provision_title.provision(
                    requested,
                    self.disc,
                    self.discdump(wrong),
                    output_dir=output,
                    identity_check=lambda _: [],
                )
            self.assertFalse((output / wrong).exists())
            self.assertFalse((output / "SYSTEM.CNF").exists())

    def test_crash3_system_cnf_not_bundled_spyro_is_authoritative(self) -> None:
        expected = provision_title.expected_executable(CRASH3)
        output = self.root / "crash3-spyro-boot"
        with self.assertRaisesRegex(provision_title.Refused, f"SPYRO.EXE.*{expected}"):
            provision_title.provision(
                CRASH3,
                self.disc,
                self.discdump("DRAGON/SPYRO.EXE", extra_executables=(expected,)),
                output_dir=output,
                identity_check=lambda _: [],
            )
        self.assertFalse((output / expected).exists())
        self.assertFalse((output / "SPYRO.EXE").exists())
        self.assertFalse((output / "SYSTEM.CNF").exists())


if __name__ == "__main__":
    unittest.main()
