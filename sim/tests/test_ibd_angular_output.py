#!/usr/bin/env python3
"""End-to-end check for IBD angular kinematics output."""

from __future__ import annotations

import argparse
import math
import subprocess
import tempfile
from pathlib import Path


def _rows(path: Path):
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            yield line.split()


def _assert_unit_vector(values: list[float], label: str) -> None:
    norm = math.sqrt(sum(v * v for v in values))
    assert abs(norm - 1.0) < 1e-5, f"{label} is not unit length: {norm}"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--events", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=45.0)
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[2]
    sim = repo / "sim" / "build" / "sim"
    assert sim.exists(), f"simulation executable not found: {sim}"

    macro_text = f"""
/control/verbose 0
/run/verbose 0
/tracking/verbose 0
/run/initialize
/analysis/writeCherenkovSpectrum false
/gen/mode ibd
/gen/ibdAngularModel vogelBeacomTable
/gen/ibdNuDir 0 0 -1
/gen/ibdEmitNeutron false
/run/beamOn {args.events}
"""

    with tempfile.TemporaryDirectory(prefix="ibd-angular-") as tmp:
        run_dir = Path(tmp)
        macro = run_dir / "ibd_angular.mac"
        macro.write_text(macro_text, encoding="utf-8")

        try:
            result = subprocess.run(
                [str(sim), str(macro)],
                cwd=run_dir,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=args.timeout,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            stdout = exc.stdout or ""
            raise AssertionError(f"simulation timed out after {args.timeout}s\n{stdout[-4000:]}")
        assert result.returncode == 0, result.stdout[-4000:]

        kin_path = run_dir / "ibd_kinematics.txt"
        assert kin_path.exists(), f"missing angular output file; log tail:\n{result.stdout[-4000:]}"

        parsed = []
        for parts in _rows(kin_path):
            assert len(parts) >= 15, f"expected at least 15 columns, got {len(parts)}: {parts}"
            parsed.append([float(x) for x in parts])

        assert len(parsed) == args.events, f"expected {args.events} kinematics rows, got {len(parsed)}"

        cos_e_values = []
        cos_n_values = []
        for row in parsed:
            _, enu, ee, te, cos_e, _, ex, ey, ez, tn, cos_n, _, nx, ny, nz = row[:15]
            assert enu > 1.806, f"Enu below IBD threshold: {enu}"
            assert ee > 0.511, f"invalid positron total energy: {ee}"
            assert te >= 0.0, f"invalid positron kinetic energy: {te}"
            assert -1.0 <= cos_e <= 1.0, f"cosThetaE out of range: {cos_e}"
            assert tn >= 0.0, f"invalid neutron kinetic energy: {tn}"
            assert -1.0 <= cos_n <= 1.0, f"cosThetaN out of range: {cos_n}"
            _assert_unit_vector([ex, ey, ez], "positron direction")
            _assert_unit_vector([nx, ny, nz], "neutron direction")
            cos_e_values.append(cos_e)
            cos_n_values.append(cos_n)

        mean_cos_e = sum(cos_e_values) / len(cos_e_values)
        mean_cos_n = sum(cos_n_values) / len(cos_n_values)
        assert -0.8 < mean_cos_e < 0.8, f"positron angular sample is implausibly one-sided: {mean_cos_e}"
        assert mean_cos_n > 0.5, f"neutron direction should be forward-peaked: {mean_cos_n}"

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
