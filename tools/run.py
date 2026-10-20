#!/usr/bin/env python3
"""Provision, build, and launch the Crash 1 native/Lightrec product."""

from __future__ import annotations

import os
import sys
from collections.abc import Sequence
from pathlib import Path

from port import launcher
from port.launch_environment import player_environment

ROOT = Path(__file__).resolve().parents[1]


def launch(disc: str | None, prepare_only: bool) -> None:
    launcher.preflight()
    discdump = launcher.build_discdump(ROOT, os.environ)
    launcher.run([sys.executable, "tools/provision_title.py", "--title", "crash1", "--discdump", discdump,
                  *([disc] if disc else [])], os.environ, cwd=ROOT)
    build = launcher.build_product(ROOT, "crash_port", os.environ)
    if prepare_only:
        launcher.say("Crash 1 is built and ready.")
        return
    launcher.exec_product(build / "crash_port", [], player_environment(os.environ, product="crash"), ROOT)


def main(argv: Sequence[str] | None = None) -> int:
    args = launcher.argument_parser(__doc__).parse_args(argv)
    return launcher.main(lambda: launch(args.disc, args.prepare_only))


if __name__ == "__main__":
    raise SystemExit(main())
