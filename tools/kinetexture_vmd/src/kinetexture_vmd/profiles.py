"""Explicit compatible-local profiles, not an arbitrary-skeleton retargeter."""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

from .parser import VmdError
from .sampling import multiply, normalize, sample_track


def read_json(path, *, with_hash=False):
    def no_constant(value):
        raise VmdError(f"non-finite JSON constant: {value}")
    def unique(pairs):
        result = {}
        for name, value in pairs:
            if name in result:
                raise VmdError(f"duplicate profile JSON key: {name}")
            result[name] = value
        return result
    path = Path(path)
    if path.stat().st_size > 1024 * 1024:
        raise VmdError("profile JSON byte budget exceeded")
    with path.open("rb") as stream:
        data = stream.read(1024*1024+1)
    if len(data) > 1024*1024:
        raise VmdError("profile JSON byte budget exceeded")
    value = json.loads(data.decode("utf-8"), parse_constant=no_constant,
                       object_pairs_hook=unique)
    return (value, hashlib.sha256(data).hexdigest()) if with_hash else value


def file_hash(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _vector(value, length, label):
    if not isinstance(value, list) or len(value) != length or not all(
            isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v)
            for v in value):
        raise VmdError(f"{label} must contain {length} finite numbers")
    return value


def validate_profile(profile, role):
    if not isinstance(profile, dict) or profile.get("schema") not in (f"kinetexture.vmd.{role}/1", f"motionweave.vmd.{role}/1"):
        raise VmdError(f"invalid {role} profile schema")
    for field in ("id", "revision", "basis"):
        if not isinstance(profile.get(field), str) or not profile[field].strip():
            raise VmdError(f"{role} profile requires {field}")
    expected = "prebaked_local" if role == "source" else "compatible_local"
    if profile.get("evaluation") != expected:
        raise VmdError(f"only explicit {expected} evaluation is supported; no IK/append/retarget")
    joints = profile.get("joints")
    if not isinstance(joints, list) or not 0 < len(joints) <= 256:
        raise VmdError("profile requires 1..256 joints")
    names = set()
    for joint in joints:
        if not isinstance(joint, dict):
            raise VmdError("joint must be an object")
        name = joint.get("name")
        if not isinstance(name, str) or not name or name in names:
            raise VmdError("duplicate/empty joint name")
        names.add(name)
        _vector(joint.get("rest_offset"), 3, "rest_offset")
        rotation = _vector(joint.get("rest_rotation"), 4, "rest_rotation")
        if abs(sum(v*v for v in rotation)-1) > 1e-6:
            raise VmdError("rest_rotation must be unit quaternion xyzw")
        if "parent" not in joint:
            raise VmdError("joint parent must be explicit (null for a root)")
    parents = {j["name"]: j["parent"] for j in joints}
    for name, parent in parents.items():
        if parent is not None and (not isinstance(parent, str) or parent not in names):
            raise VmdError("unknown profile parent")
        seen = set()
        current = name
        while current is not None:
            if current in seen:
                raise VmdError("cyclic profile parent chain")
            seen.add(current)
            current = parents[current]
    if sum(p is None for p in parents.values()) != 1:
        raise VmdError("compatible-local preview requires exactly one root")
    return profile


def sample_compatible(vmd, source, target, *, max_joint_samples=2_000_000):
    """Same topology/rest/coordinate frame only; caller supplies both profiles.

    Source offsets and target offsets are in meters. VMD position values use
    source.meters_per_unit. Local absolute rotation = rest * VMD delta. FPS is
    source-authored metadata; VMD has no FPS field. No basis conversion, IK,
    append/grant, morph/camera evaluation or arbitrary retargeting is provided.
    The returned dictionary is diagnostic/test interchange, not a disk format.
    """
    validate_profile(source, "source")
    validate_profile(target, "target")
    if source["basis"] != target["basis"]:
        raise VmdError("coordinate basis mismatch: basis conversion is not implemented")
    for name in ("fps", "meters_per_unit"):
        value = source.get(name)
        if not isinstance(value, (int, float)) or isinstance(value, bool) or not math.isfinite(value) or value <= 0:
            raise VmdError(f"source profile requires finite positive {name}")
    if not 0.001 <= source["fps"] <= 1000 or source["meters_per_unit"] > 1000:
        raise VmdError("source FPS/unit scale exceeds preview budget")
    if vmd.morphs or vmd.cameras or vmd.properties or vmd.light_count or vmd.shadow_count:
        raise VmdError("compatible-local export rejects morph/camera/light/shadow/IK/property tracks")
    source_joints = {j["name"]: j for j in source["joints"]}
    tracks = [j.get("track") for j in source["joints"]]
    if any(not isinstance(t, str) or not t for t in tracks) or len(set(tracks)) != len(tracks):
        raise VmdError("source profile requires unique explicit VMD track names")
    if set(vmd.bones) - set(tracks):
        raise VmdError("VMD has unmapped source bone tracks")
    mapped = [j.get("source") for j in target["joints"]]
    if len(mapped) != len(source_joints) or set(mapped) != set(source_joints):
        raise VmdError("target must map every source joint exactly once")
    target_names = {j["name"]: j for j in target["joints"]}
    for joint in target["joints"]:
        original = source_joints[joint["source"]]
        parent_source = (target_names[joint["parent"]]["source"]
                         if joint["parent"] is not None else None)
        if parent_source != original["parent"]:
            raise VmdError("parent topology differs: retargeting is not implemented")
        for field in ("rest_offset", "rest_rotation"):
            if any(abs(a-b) > 1e-8 for a, b in zip(joint[field], original[field])):
                raise VmdError("rest geometry differs: retargeting is not implemented")
    frames = vmd.max_frame + 1
    if frames * len(target["joints"]) > max_joint_samples:
        raise VmdError("dense joint sample budget exceeded")
    root = next(j for j in target["joints"] if j["parent"] is None)
    root_source = source_joints[root["source"]]
    first = sample_track(vmd.bones.get(root_source["track"], []), 0)
    root_start = first[0] if first else (0, 0, 0)
    scale = source["meters_per_unit"]
    sampled = []
    for frame in range(frames):
        rotations, positions, valid, root_position = [], [], [], [0, 0, 0]
        for joint in target["joints"]:
            original = source_joints[joint["source"]]
            sample = sample_track(vmd.bones.get(original["track"], []), frame)
            pos, rotation = sample if sample else ((0, 0, 0), (0, 0, 0, 1))
            rotations.append(normalize(multiply(original["rest_rotation"], rotation)))
            valid.append(sample is not None)
            if joint["parent"] is None:
                root_position = [(a-b)*scale for a, b in zip(pos, root_start)]
                positions.append(list(joint["rest_offset"]))
            else:
                positions.append([a+b*scale for a, b in zip(joint["rest_offset"], pos)])
        sampled.append({"time_seconds": frame/source["fps"],
                        "root_position": root_position, "local_rotations": rotations,
                        "local_translations": positions, "valid": valid})
    return {"diagnostic_schema": "kinetexture.vmd.samples/1", "not_a_distribution_format": True,
            "fps": source["fps"], "frame_count": frames, "basis": target["basis"],
            "rotation_space": "local_absolute", "quaternion": "xyzw", "unit": "meters",
            "root_frame": "clip_relative_to_frame_zero", "joints": target["joints"],
            "source_profile": {k: source[k] for k in ("id", "revision")},
            "target_profile": {k: target[k] for k in ("id", "revision")}, "frames": sampled}
