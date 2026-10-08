#!/usr/bin/env python3
"""Run Crash's asset-free product and title contracts through PSXPort's shared verifier."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PSXPORT = ROOT / "external" / "psxport"
BUILD = ROOT / "build" / "verify"


def main() -> int:
    bootstrap = subprocess.run(
        [sys.executable, ROOT / "tools" / "psxport_fetch.py", "--auto"],
        cwd=ROOT,
        check=False,
    )
    if bootstrap.returncode:
        return bootstrap.returncode

    sys.path.insert(0, str(PSXPORT / "tools"))
    from port.consumer_verify import ConsumerVerifyConfig, run_consumer_verification

    result = run_consumer_verification(
        ConsumerVerifyConfig(
            name="Crash Bandicoot",
            root=ROOT,
            build=BUILD,
            psxport=PSXPORT,
            product=BUILD / "crash1_port",
            cmake_module=ROOT / "CMakeLists.txt",
            test_regex=r"^crash",
            cmake_definitions=("-DBUILD_TESTING=ON", "-DPSXPORT_BUILD_SMOKE=OFF"),
        )
    )
    if result:
        return result
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
