"""Bounded VMD 0002 parsing. No rig evaluation is inferred from a VMD file.

Binary field layouts were checked against the repository's vmd_parser.py.
This reader adds explicit budgets, exact reads, finite checks and duplicate
rejection; it does not import that module or its target data.
"""
from __future__ import annotations

from dataclasses import dataclass, field
import hashlib
import math
from pathlib import Path
import struct


class VmdError(ValueError):
    """Malformed input or an explicitly unsupported conversion contract."""


@dataclass(frozen=True)
class BoneKey:
    frame: int
    position: tuple[float, ...]
    rotation: tuple[float, ...]
    curves: tuple[tuple[float, ...], ...]


@dataclass
class Vmd:
    model_name: str
    source_sha256: str = ""
    bones: dict[str, list[BoneKey]] = field(default_factory=dict)
    morphs: dict[str, list[tuple[int, float]]] = field(default_factory=dict)
    cameras: list[dict] = field(default_factory=list)
    properties: list[dict] = field(default_factory=list)
    light_count: int = 0
    shadow_count: int = 0
    max_frame: int = 0


class _Reader:
    def __init__(self, data: bytes, max_keys: int, max_frame: int):
        self.data, self.offset = data, 0
        self.max_keys, self.max_frame, self.keys = max_keys, max_frame, 0

    @property
    def remaining(self):
        return len(self.data) - self.offset

    def take(self, size: int):
        if size < 0 or size > self.remaining:
            raise VmdError(f"truncated VMD at byte {self.offset}: need {size} bytes")
        value = self.data[self.offset:self.offset + size]
        self.offset += size
        return value

    def unpack(self, fmt: str):
        return struct.unpack(fmt, self.take(struct.calcsize(fmt)))

    def text(self, length: int):
        raw = self.take(length).split(b"\0", 1)[0]
        try:
            value = raw.decode("shift_jis")
        except UnicodeError as exc:
            raise VmdError(f"invalid Shift-JIS name at byte {self.offset - length}") from exc
        if not value or any(ord(c) < 32 for c in value):
            raise VmdError("empty/control character VMD name")
        return value

    def count(self, record_size: int):
        count, = self.unpack("<I")
        self.keys += count
        if self.keys > self.max_keys:
            raise VmdError("VMD key budget exceeded")
        if count > self.remaining // record_size:
            raise VmdError(f"truncated VMD section with {count} records")
        return count

    def frame(self):
        value, = self.unpack("<I")
        if value > self.max_frame:
            raise VmdError("VMD frame budget exceeded")
        return value

    def floats(self, count):
        values = self.unpack("<" + "f" * count)
        if not all(math.isfinite(v) for v in values):
            raise VmdError("non-finite VMD scalar")
        return values


def parse_vmd(path, *, max_bytes=64 * 1024 * 1024, max_keys=500_000,
              max_frame=1_000_000) -> Vmd:
    """Read 0002 only; missing optional sections are accepted at exact boundaries.

    Duplicate keys at the same track/frame, invalid quaternions, malformed
    optional sections, unknown trailing bytes and oversized inputs fail closed.
    Camera/light/shadow/IK values are inspected but are not sampled by the
    compatible-local exporter.
    """
    path = Path(path)
    if path.stat().st_size > max_bytes:
        raise VmdError("VMD byte budget exceeded")
    with path.open("rb") as stream:
        data = stream.read(max_bytes + 1)
    if len(data) > max_bytes:
        raise VmdError("VMD byte budget exceeded")
    r = _Reader(data, max_keys, max_frame)
    if r.take(30).rstrip(b"\0") != b"Vocaloid Motion Data 0002":
        raise VmdError("only VMD 'Vocaloid Motion Data 0002' is supported")
    vmd = Vmd(r.text(20), source_sha256=hashlib.sha256(data).hexdigest())
    for _ in range(r.count(111)):
        name, frame = r.text(15), r.frame()
        position, rotation = r.floats(3), r.floats(4)
        norm = sum(x * x for x in rotation)
        if norm < 1e-12 or abs(norm - 1.0) > 0.01:
            raise VmdError("VMD quaternion must have nonzero, near-unit norm")
        curve = r.take(64)
        if any(c > 127 for c in curve[:16]):
            raise VmdError("VMD Bezier control byte outside [0,127]")
        curves = tuple(tuple(curve[i + j] / 127 for j in (0, 4, 8, 12)) for i in range(4))
        vmd.bones.setdefault(name, []).append(BoneKey(frame, position, rotation, curves))
        vmd.max_frame = max(vmd.max_frame, frame)
    if r.remaining:
        for _ in range(r.count(23)):
            name, frame = r.text(15), r.frame()
            vmd.morphs.setdefault(name, []).append((frame, r.floats(1)[0]))
            vmd.max_frame = max(vmd.max_frame, frame)
    if r.remaining:
        for _ in range(r.count(61)):
            frame, values, curves = r.frame(), r.floats(7), r.take(24)
            fov, perspective = r.unpack("<IB")
            if perspective not in (0, 1) or fov > 180:
                raise VmdError("invalid VMD camera perspective/FOV")
            vmd.cameras.append({"frame": frame, "values": values,
                                "curves": curves.hex(), "fov": fov,
                                "perspective": perspective == 0})
            vmd.max_frame = max(vmd.max_frame, frame)
    if r.remaining:
        vmd.light_count = r.count(28)
        for _ in range(vmd.light_count):
            vmd.max_frame = max(vmd.max_frame, r.frame())
            r.floats(6)
    if r.remaining:
        vmd.shadow_count = r.count(9)
        for _ in range(vmd.shadow_count):
            vmd.max_frame = max(vmd.max_frame, r.frame())
            mode, = r.unpack("<B")
            if mode > 2:
                raise VmdError("invalid VMD shadow mode")
            r.floats(1)
    if r.remaining:
        for _ in range(r.count(9)):
            frame = r.frame()
            visible, = r.unpack("<B")
            if visible not in (0, 1):
                raise VmdError("invalid visibility flag")
            switches = {}
            for _ in range(r.count(21)):
                name = r.text(20)
                enabled, = r.unpack("<B")
                if name in switches or enabled not in (0, 1):
                    raise VmdError("duplicate or invalid VMD IK switch")
                switches[name] = bool(enabled)
            vmd.properties.append({"frame": frame, "visible": bool(visible), "ik": switches})
            vmd.max_frame = max(vmd.max_frame, frame)
    if r.remaining:
        raise VmdError("unknown trailing VMD data")
    tracks = list(vmd.bones.values())
    for keys in tracks:
        keys.sort(key=lambda k: k.frame)
        if len({k.frame for k in keys}) != len(keys):
            raise VmdError("duplicate bone key at one frame")
    for keys in vmd.morphs.values():
        keys.sort()
        if len({k[0] for k in keys}) != len(keys):
            raise VmdError("duplicate morph key at one frame")
    for keys in (vmd.cameras, vmd.properties):
        keys.sort(key=lambda k: k["frame"])
        if len({k["frame"] for k in keys}) != len(keys):
            raise VmdError("duplicate camera/property key at one frame")
    return vmd
