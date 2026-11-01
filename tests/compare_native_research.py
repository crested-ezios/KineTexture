"""Research-only differential gate. The C++ library does not invoke Python.

Run against a built kinetexture_native_tests executable. The executable emits
its deterministic inputs and native outputs; this script evaluates those same
inputs with the retained Python research package. No oracle cache is rewritten.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys

import numpy as np
from scipy.spatial.transform import Rotation

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "python" / "src"))
from kinetexture.ik import solve_two_bone
from kinetexture.retarget import (
    CoordinateTransform, align_bone_frames, compose_rotation_delta,
    local_to_world_rotations, rebase_absolute_rotation, rebase_rotation_delta,
    world_to_local_rotations,
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    completed = subprocess.run([str(args.executable.resolve()), "--oracle"],
                               check=True, text=True, capture_output=True)
    maxima = {"vector": 0.0, "rotation_radians": 0.0, "scalar": 0.0}
    counts = {"ik": 0, "retarget": 0, "hierarchy": 0}

    def vector(a, b):
        error = float(np.max(np.abs(np.asarray(a) - np.asarray(b))))
        maxima["vector"] = max(maxima["vector"], error)
        if not np.isfinite(error) or error > 2e-9:
            raise AssertionError(f"vector error {error}: {a} vs {b}")

    def rotation(a, b):
        error = float(np.max((Rotation.from_quat(a).inv() * Rotation.from_quat(b)).magnitude()))
        maxima["rotation_radians"] = max(maxima["rotation_radians"], error)
        if not np.isfinite(error) or error > 2e-9:
            raise AssertionError(f"rotation error {error}: {a} vs {b}")

    def scalar(a, b):
        error = abs(float(a) - float(b))
        maxima["scalar"] = max(maxima["scalar"], error)
        if not np.isfinite(error) or error > 2e-9:
            raise AssertionError(f"scalar error {error}: {a} vs {b}")

    for line in completed.stdout.splitlines():
        row = json.loads(line)
        kind = row["kind"]
        counts[kind] += 1
        if kind == "ik":
            expected = solve_two_bone(row["root"], row["goal"], row["upper_offset"],
                                      row["lower_offset"], row["upper_hint"], row["lower_hint"],
                                      previous_pole=row["previous"], pole_hint=row["explicit"])
            rotation(row["upper"], expected.upper_world.as_quat())
            rotation(row["lower"], expected.lower_world.as_quat())
            vector(row["joint"], expected.joint_position)
            vector(row["end"], expected.end_position)
            vector(row["pole"], expected.pole)
            scalar(row["requested"], expected.requested_distance)
            scalar(row["distance"], expected.reachable_distance)
            scalar(row["residual"], expected.residual)
            if row["reachable"] != expected.reachable:
                raise AssertionError("reachability disagreement")
        elif kind == "retarget":
            transform = CoordinateTransform(row["basis"], 0.08)
            vector(row["vector"], transform.vectors(row["input_vector"]))
            # Coordinate conversion retains the input norm as well as rotation.
            vector(row["rotation"], transform.rotations(row["input_rotation"]))
            k = align_bone_frames(row["source_primary"], row["source_hinge"],
                                  row["target_primary"], row["target_hinge"])
            rotation(row["k"], k)
            rotation(row["absolute"], rebase_absolute_rotation(row["input_rotation"], k))
            rotation(row["delta"], rebase_rotation_delta(row["input_rotation"], k))
            rotation(row["local"], compose_rotation_delta(row["input_rotation"], row["change"], space="local"))
            rotation(row["parent"], compose_rotation_delta(row["input_rotation"], row["change"], space="parent"))
        elif kind == "hierarchy":
            parents = [2, -1, 1]
            worlds = local_to_world_rotations(row["locals"], parents)
            rotation(row["worlds"], worlds)
            rotation(row["roundtrip"], world_to_local_rotations(worlds, parents))
        else:
            raise AssertionError(f"unknown native record {kind}")
    if counts != {"ik": 128, "retarget": 128, "hierarchy": 128}:
        raise AssertionError(f"unexpected fixture counts {counts}")
    report = {"status": "passed", "records": counts, "max_errors": maxima,
              "tolerance": 2e-9, "gate": "offline numerical equivalence; no runtime or visual claim"}
    text = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    print(text, end="")


if __name__ == "__main__":
    main()
