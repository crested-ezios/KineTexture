"""Explicit legacy gkclip v2 target adapter, not the neutral source parser.

HID parent topology below mirrors src/GkMotionData.hpp::GakumasParentHid at
source baseline c4a9e6588eff730e93efe8cfcc1b9be68cd7b2b0. It contains no
character rest geometry, extracted facial data or motion. File byte layout is
the existing tools/vmd2gkmotion/gkclip_format.py writer contract.
"""
from __future__ import annotations

import struct

from .parser import VmdError


PARENTS = {0: -1, 7: 0, 8: 7, 54: 8, 9: 54, 10: 9, 11: 54, 12: 54,
           13: 11, 14: 12, 15: 13, 16: 14, 17: 15, 18: 16, 1: 0, 2: 0,
           3: 1, 4: 2, 5: 3, 6: 4, 19: 5, 20: 6}
for first, hand in ((24, 17), (39, 18)):
    for finger in range(5):
        start = first + finger*3
        PARENTS.update({start: hand, start+1: start, start+2: start+1})


def serialize_gkclip(samples, target):
    """Only identity-rest, already-baked compatible locals can be exported.

    No name inference, implicit target profile, IK, translation dropping or
    hidden generic retargeting. A sidecar supplied by CLI records the explicit
    target geometry/profile because the legacy binary has no rest/parent table.
    """
    if target.get("output_contract") != "gkclip_v2_canonical_local":
        raise VmdError("target must explicitly request gkclip_v2_canonical_local")
    joints = target["joints"]
    ids = [j.get("humanoid_id") for j in joints]
    if any(not isinstance(i, int) or isinstance(i, bool) or i not in PARENTS for i in ids) or len(set(ids)) != len(ids):
        raise VmdError("unsupported/duplicate target humanoid ID")
    by_name = {j["name"]: j for j in joints}
    for joint in joints:
        rotation = joint["rest_rotation"]
        if sum(x*x for x in rotation[:3]) > 1e-16 or abs(abs(rotation[3])-1) > 1e-8:
            raise VmdError("gkclip compatible-local preview requires identity rest rotations")
        hid = joint["humanoid_id"]
        expected = PARENTS[hid] if PARENTS[hid] in ids else -1
        parent = by_name[joint["parent"]]["humanoid_id"] if joint["parent"] is not None else -1
        if parent != expected:
            raise VmdError("target topology differs from legacy gkclip consumer")
    for frame in samples["frames"]:
        if not all(frame["valid"]):
            raise VmdError("gkclip export requires every target bone track; missing is not identity")
        for joint, position in zip(joints, frame["local_translations"]):
            if any(abs(a-b) > 1e-8 for a, b in zip(position, joint["rest_offset"])):
                raise VmdError("gkclip cannot carry non-root local translations")
    count, fps = samples["frame_count"], samples["fps"]
    result = bytearray(b"GKCLIPCL")
    result += struct.pack("<IfIfIf", 2, fps, count, (count-1)/fps, len(joints), 1.0)
    for joint in joints:
        name = joint["name"].encode("utf-8")
        if not 0 < len(name) <= 255:
            raise VmdError("gkclip target name length must be 1..255 UTF-8 bytes")
        result += struct.pack("<HBB", joint["humanoid_id"], len(name), 1) + name
    try:
        for frame in samples["frames"]:
            result += struct.pack("<3f", *frame["root_position"])
            for rotation in frame["local_rotations"]:
                result += struct.pack("<4f", *rotation)
    except (OverflowError, struct.error) as exc:
        raise VmdError("sample exceeds finite float32 payload range") from exc
    result += b"\0"  # camera absent; no morph channels in v2
    return bytes(result)
