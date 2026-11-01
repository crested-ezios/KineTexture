"""Self-authored two-joint synthetic input; no real model/music/motion assets."""
from __future__ import annotations

import json
import math
from pathlib import Path
import struct


def make_fixture(output):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=False)
    source = {"schema": "kinetexture.vmd.source/1", "id": "synthetic-two-joint-source",
              "revision": "1", "basis": "synthetic_y_up", "fps": 30,
              "meters_per_unit": 1, "evaluation": "prebaked_local", "joints": [
                  {"name": "root", "parent": None, "track": "Root", "rest_offset": [0, 0, 0], "rest_rotation": [0, 0, 0, 1]},
                  {"name": "child", "parent": "root", "track": "Child", "rest_offset": [0, 0.5, 0], "rest_rotation": [0, 0, 0, 1]}]}
    target = {"schema": "kinetexture.vmd.target/1", "id": "synthetic-two-joint-target",
              "revision": "1", "basis": "synthetic_y_up", "evaluation": "compatible_local",
              "output_contract": "gkclip_v2_canonical_local", "joints": [
                  {"name": "Hips", "source": "root", "parent": None, "humanoid_id": 0, "rest_offset": [0, 0, 0], "rest_rotation": [0, 0, 0, 1]},
                  {"name": "Spine", "source": "child", "parent": "Hips", "humanoid_id": 7, "rest_offset": [0, 0.5, 0], "rest_rotation": [0, 0, 0, 1]}]}
    curve = bytes([20]*8 + [107]*8 + [0]*48)
    data = bytearray(b"Vocaloid Motion Data 0002".ljust(30, b"\0") + b"Synthetic".ljust(20, b"\0"))
    data += struct.pack("<I", 4)
    for name, axis, degrees in (("Root", 2, 90), ("Child", 0, 60)):
        for frame in (0, 2):
            angle = math.radians(degrees*frame/2)/2
            rotation = [0.0, 0.0, 0.0, math.cos(angle)]
            rotation[axis] = math.sin(angle)
            position = [float(frame), 0.0, 0.0] if name == "Root" else [0.0]*3
            data += name.encode("ascii").ljust(15, b"\0")
            data += struct.pack("<I3f4f", frame, *position, *rotation) + curve
    data += struct.pack("<5I", 0, 0, 0, 0, 0)
    (output/"synthetic.vmd").write_bytes(data)
    for name, profile in (("source.json", source), ("target.json", target)):
        (output/name).write_text(json.dumps(profile, indent=2)+"\n", encoding="utf-8")
    return output
