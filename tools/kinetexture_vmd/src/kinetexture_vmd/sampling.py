"""VMD bone interpolation, independent of all rig/target implementations.

Bezier bisection follows tools/vmd2gkmotion/bezier.py (20 iterations, destination
key curves). Quaternion interpolation is normalized shortest-arc SLERP.
"""
from __future__ import annotations

import bisect
import math

from .parser import VmdError


def bezier(curve, x):
    if x <= 0:
        return 0.0
    if x >= 1:
        return 1.0
    x1, y1, x2, y2 = curve
    low, high = 0.0, 1.0
    for _ in range(20):
        t = (low + high) * 0.5
        a = 1.0 - t
        bx = 3 * a * a * t * x1 + 3 * a * t * t * x2 + t ** 3
        if bx < x:
            low = t
        else:
            high = t
    t = (low + high) * 0.5
    a = 1.0 - t
    return 3 * a * a * t * y1 + 3 * a * t * t * y2 + t ** 3


def normalize(q):
    norm = math.sqrt(sum(x * x for x in q))
    if not math.isfinite(norm) or norm < 1e-12:
        raise VmdError("invalid quaternion")
    return tuple(x / norm for x in q)


def multiply(a, b):
    x, y, z, w = a
    u, v, s, t = b
    return (w*u + x*t + y*s - z*v, w*v - x*s + y*t + z*u,
            w*s + x*v - y*u + z*t, w*t - x*u - y*v - z*s)


def slerp(a, b, t):
    a, b = normalize(a), normalize(b)
    dot = sum(x * y for x, y in zip(a, b))
    if dot < 0:
        b, dot = tuple(-x for x in b), -dot
    dot = min(1.0, max(-1.0, dot))
    if dot > 0.9999995:
        return normalize(tuple(x + (y-x)*t for x, y in zip(a, b)))
    angle = math.acos(dot)
    weight_a = math.sin((1-t)*angle) / math.sin(angle)
    weight_b = math.sin(t*angle) / math.sin(angle)
    return normalize(tuple(x*weight_a + y*weight_b for x, y in zip(a, b)))


def sample_track(keys, frame: float):
    """Sample source-local translation/rotation. Ends hold; no implicit FPS.

    Empty tracks return None, preserving missing versus identity. Callers decide
    the missing-track policy. The same destination-key Bezier convention as the
    legacy sampler is used, with normalized endpoint quaternions.
    """
    if not math.isfinite(frame):
        raise VmdError("sample frame must be finite")
    if not keys:
        return None
    index = bisect.bisect_right(keys, frame, key=lambda k: k.frame) - 1
    if index < 0:
        return keys[0].position, normalize(keys[0].rotation)
    if index >= len(keys)-1:
        return keys[-1].position, normalize(keys[-1].rotation)
    a, b = keys[index:index+2]
    t = (frame-a.frame)/(b.frame-a.frame)
    factors = [bezier(c, t) for c in b.curves]
    position = tuple(x + (y-x)*k for x, y, k in zip(a.position, b.position, factors))
    return position, slerp(a.rotation, b.rotation, factors[3])
