"""Analytical two-bone IK on explicit chain geometry.

All positions and offsets use the same units. Rotations are world rotations,
and offsets are child positions in their parent's local frame. This module
does not infer skeleton axes, source scale, foot contact or root placement.
Unreachable goals produce a physically reachable FK endpoint and a residual;
the end is never assigned a position inconsistent with its parent rotation.
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from scipy.spatial.transform import Rotation


@dataclass(frozen=True)
class TwoBoneSolution:
    upper_world: Rotation
    lower_world: Rotation
    joint_position: np.ndarray
    end_position: np.ndarray
    pole: np.ndarray
    requested_distance: float
    reachable_distance: float
    residual: float
    reachable: bool


def _vector(value, name):
    vector = np.asarray(value, dtype=np.float64)
    if vector.shape != (3,) or not np.isfinite(vector).all():
        raise ValueError(f"{name} must be a finite three-component vector")
    return vector


def _rotation(value, name):
    if isinstance(value, Rotation):
        quaternion = value.as_quat()
    else:
        quaternion = np.asarray(value, dtype=np.float64)
    norm = np.linalg.norm(quaternion)
    if (quaternion.shape != (4,) or not np.isfinite(quaternion).all()
            or not np.isfinite(norm) or norm < 1e-12):
        raise ValueError(f"{name} must be one finite, nonzero XYZW quaternion")
    return Rotation.from_quat(quaternion)


def _local_orthogonal(direction):
    """Deterministic local-frame axis, so rigid world transforms commute."""
    reference = np.eye(3)[int(np.argmin(np.abs(direction)))]
    axis = np.cross(direction, reference)
    return axis / np.linalg.norm(axis)


def _aim_offset(hint, offset_unit, desired_direction):
    """Minimal world-space swing of a hint; no added twist about the segment."""
    current = hint.apply(offset_unit)
    cross = np.cross(current, desired_direction)
    cross_length = float(np.linalg.norm(cross))
    dot = float(np.clip(np.dot(current, desired_direction), -1.0, 1.0))
    if cross_length < 1e-12:
        if dot >= 0.0:
            return hint
        # At exactly 180 degrees the shortest arc is nonunique. Derive its
        # axis from the hint's own local frame, never a fixed world axis.
        axis = hint.apply(_local_orthogonal(offset_unit))
        swing = Rotation.from_rotvec(axis * np.pi)
    else:
        angle = np.arctan2(cross_length, dot)
        swing = Rotation.from_rotvec(cross * (angle / cross_length))
    return swing * hint


def _bend_pole(ray, upper_direction, lower_direction, previous, local_fallback, explicit):
    # The original joint lies along upper_direction from the root. Project it
    # onto the goal-normal plane to retain the source bend side. At a straight
    # pose, prefer the preceding frame before resorting to an intrinsic axis.
    if explicit is not None:
        # An explicit source hinge/pole is independent of the old joint's
        # projection onto a differently scaled target ray. That projection can
        # cross zero and flip the inferred bend side between adjacent frames.
        candidates = [explicit]
        if previous is not None:
            candidates.append(previous)
        candidates.extend((local_fallback, np.cross(upper_direction, local_fallback)))
    else:
        candidates = [upper_direction]
        if previous is not None:
            candidates.append(previous)
        candidates.extend((-lower_direction, local_fallback))
    for candidate in candidates:
        with np.errstate(over='ignore', invalid='ignore'):
            projected = candidate - ray * np.dot(candidate, ray)
            norm = float(np.linalg.norm(projected))
        if not np.isfinite(projected).all() or not np.isfinite(norm):
            raise ValueError('pole projection exceeds the finite numeric range')
        if norm > 1e-8:
            return projected / norm
    # The intrinsic basis spans two orthogonal axes. They cannot both be
    # parallel to a finite ray; the default branch also has the upper hint.
    raise ValueError("could not construct a bend plane from the rotation hints")


def solve_two_bone(
    root_position,
    goal_position,
    upper_offset,
    lower_offset,
    upper_world_hint,
    lower_world_hint,
    *,
    previous_pole=None,
    pole_hint=None,
) -> TwoBoneSolution:
    """Solve arbitrary local axes and unequal segment lengths in world space.

    ``upper_offset`` is the joint's local position in the upper segment;
    ``lower_offset`` is the end's local position in the lower segment. The hint
    rotations retain the joint bend side and each segment's existing roll.
    ``pole_hint`` optionally supplies a world bend direction independent of
    the hint rotations' projected joint position; it takes precedence after
    projection onto the goal-normal plane. If that direction is degenerate,
    use ``previous_pole`` and then an intrinsic rotation-hint axis.
    ``previous_pole`` is a world direction returned by the preceding solve,
    used if the current hint is straight. A caller must reset it at cuts or
    IK-disabled intervals. The solution clamps the radial goal distance to
    ``[abs(L1-L2), L1+L2]`` and reports the resulting endpoint residual.
    """
    root = _vector(root_position, "root_position")
    goal = _vector(goal_position, "goal_position")
    upper_offset = _vector(upper_offset, "upper_offset")
    lower_offset = _vector(lower_offset, "lower_offset")
    previous = None if previous_pole is None else _vector(previous_pole, "previous_pole")
    explicit_pole = None if pole_hint is None else _vector(pole_hint, "pole_hint")
    upper_hint = _rotation(upper_world_hint, "upper_world_hint")
    lower_hint = _rotation(lower_world_hint, "lower_world_hint")
    upper_length = float(np.linalg.norm(upper_offset))
    lower_length = float(np.linalg.norm(lower_offset))
    if not np.isfinite([upper_length, lower_length]).all():
        raise ValueError('segment lengths exceed the finite numeric range')
    if min(upper_length, lower_length) < 1e-12:
        raise ValueError("two-bone segment lengths must be positive")
    upper_unit = upper_offset / upper_length
    lower_unit = lower_offset / lower_length
    upper_direction = upper_hint.apply(upper_unit)
    lower_direction = lower_hint.apply(lower_unit)
    intrinsic_axis = upper_hint.apply(_local_orthogonal(upper_unit))

    goal_vector = goal - root
    requested = float(np.linalg.norm(goal_vector))
    if not np.isfinite(requested):
        raise ValueError('goal distance exceeds the finite numeric range')
    minimum = abs(upper_length - lower_length)
    maximum = upper_length + lower_length
    distance = float(np.clip(requested, minimum, maximum))
    if requested > 0.0:
        ray = goal_vector / requested
    else:
        # A coincident goal has no direction. The current chain supplies one,
        # preserving equivariance and avoiding an arbitrary world-space axis.
        chain = upper_length * upper_direction + lower_length * lower_direction
        chain_norm = float(np.linalg.norm(chain))
        ray = chain / chain_norm if chain_norm > maximum * 1e-12 else intrinsic_axis
    pole = _bend_pole(ray, upper_direction, lower_direction, previous, intrinsic_axis, explicit_pole)

    if distance == 0.0:
        # Equal segments can fold completely onto the root. Preserve the
        # original upper segment direction; the lower segment then points exactly back to it.
        joint = root + upper_length * upper_direction
        endpoint = root.copy()
    else:
        # Factored difference of squares avoids cancellation for nearly equal
        # lengths. The factored altitude also gives exact straight boundaries.
        along = ((upper_length - lower_length) * maximum + distance * distance) / (2.0 * distance)
        height_squared = ((maximum - distance) * (maximum + distance)
                          * (distance - minimum) * (distance + minimum)
                          / (4.0 * distance * distance))
        if not np.isfinite(along) or not np.isfinite(height_squared):
            raise ValueError('two-bone geometry exceeds the finite numeric range')
        height = np.sqrt(max(0.0, height_squared))
        joint = root + along * ray + height * pole
        endpoint = root + distance * ray

    upper_target = (joint - root) / upper_length
    lower_target = (endpoint - joint) / lower_length
    upper_world = _aim_offset(upper_hint, upper_unit, upper_target)
    lower_world = _aim_offset(lower_hint, lower_unit, lower_target)
    # Return actual FK positions, rather than an ideal goal assigned by hand.
    joint_fk = root + upper_world.apply(upper_offset)
    end_fk = joint_fk + lower_world.apply(lower_offset)
    residual = float(np.linalg.norm(goal - end_fk))
    tolerance = maximum * 1e-10
    return TwoBoneSolution(
        upper_world=upper_world,
        lower_world=lower_world,
        joint_position=joint_fk,
        end_position=end_fk,
        pole=pole,
        requested_distance=requested,
        reachable_distance=distance,
        residual=residual,
        reachable=minimum - tolerance <= requested <= maximum + tolerance,
    )
