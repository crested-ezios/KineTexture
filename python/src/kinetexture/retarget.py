"""Explicit bone-frame and coordinate transforms, with no rig/target defaults.

Quaternions are Hamilton-active xyzw; world = parent * local. Coordinate
conversion maps vectors by B and rotations by B R B^-1, including det(B)=-1.
Frame alignment K maps target-frame coordinates into source-frame coordinates:
an absolute orientation is R_source K, while a local delta is K^-1 D_source K.
These distinct operations never infer a rest pose, identity or missing value.
"""
from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from scipy.spatial.transform import Rotation


def _vector(value, name):
    out = np.asarray(value, dtype=np.float64)
    if out.shape != (3,) or not np.isfinite(out).all():
        raise ValueError(f'{name} must be one finite three-component vector')
    return out


def _quaternions(value, name='quaternions'):
    out = np.asarray(value, dtype=np.float64)
    if out.ndim < 1 or out.shape[-1] != 4 or not np.isfinite(out).all():
        raise ValueError(f'{name} must contain finite xyzw quaternions')
    norms = np.linalg.norm(out, axis=-1)
    if np.any(norms < 1e-12) or not np.isfinite(norms).all():
        raise ValueError(f'{name} must contain nonzero finite rotations')
    return out


def _basis(primary, hinge):
    x = _vector(primary, 'primary axis')
    length = np.linalg.norm(x)
    if not np.isfinite(length) or length < 1e-12:
        raise ValueError('primary axis must be nonzero')
    x = x / length
    y = _vector(hinge, 'hinge axis')
    y = y - x * np.dot(x, y)
    length = np.linalg.norm(y)
    if not np.isfinite(length) or length < 1e-8:
        raise ValueError('primary and hinge axes must not be parallel')
    y /= length
    return np.column_stack((x, y, np.cross(x, y)))


def align_bone_frames(source_primary, source_hinge, target_primary, target_hinge):
    """Return xyzw K from two explicitly declared bone frames.

    Each frame is constructed from its primary axis and the orthogonal
    projection of its hinge axis. K maps both target axes to source axes;
    declaring just a segment direction would leave twist underdetermined.
    """
    source = _basis(source_primary, source_hinge)
    target = _basis(target_primary, target_hinge)
    return Rotation.from_matrix(source @ target.T).as_quat()


def rebase_absolute_rotation(source_absolute, target_to_source_frame):
    """Map an absolute source orientation to the target bone frame: R K."""
    source = Rotation.from_quat(_quaternions(source_absolute))
    frame = Rotation.from_quat(_quaternions(target_to_source_frame))
    return (source * frame).as_quat()


def rebase_rotation_delta(source_delta, target_to_source_frame):
    """Map a delta into target-frame axes: K^-1 D K; identity stays identity."""
    delta = Rotation.from_quat(_quaternions(source_delta))
    frame = Rotation.from_quat(_quaternions(target_to_source_frame))
    return (frame.inv() * delta * frame).as_quat()


def compose_rotation_delta(reference_absolute, delta, *, space):
    """Apply an explicit delta; local => R D, parent => D R.

    The reference must be supplied even for identity. Missing data must be
    handled by the channel adapter; this operation never fills missing poses.
    """
    reference = Rotation.from_quat(_quaternions(reference_absolute))
    change = Rotation.from_quat(_quaternions(delta))
    if space == 'local':
        return (reference * change).as_quat()
    if space == 'parent':
        return (change * reference).as_quat()
    raise ValueError('delta space must be local or parent')


@dataclass(frozen=True, eq=False)
class CoordinateTransform:
    """A passive numeric-basis conversion with an explicit length scale.

    `basis` maps source coordinates into target coordinates. It must be an
    orthonormal 3x3 matrix (reflection is allowed). `length_scale` converts
    source lengths into target lengths and never scales angles. There is no
    translation, target rig, inferred handedness or automatic unit conversion.
    Input quaternions need not be normalized; their component norm is retained.
    """
    basis: np.ndarray
    length_scale: float

    def __post_init__(self):
        basis = np.array(self.basis, dtype=np.float64, copy=True)
        if (basis.shape != (3, 3) or not np.isfinite(basis).all()
                or not np.allclose(basis.T @ basis, np.eye(3), rtol=0, atol=1e-10)):
            raise ValueError('coordinate basis must be a finite orthonormal 3x3 matrix')
        scale = float(self.length_scale)
        if not np.isfinite(scale) or scale <= 0:
            raise ValueError('length scale must be positive and finite')
        basis.setflags(write=False)
        object.__setattr__(self, 'basis', basis)
        object.__setattr__(self, 'length_scale', scale)
        axial = (1.0 if np.linalg.det(basis) > 0 else -1.0) * basis
        axial.setflags(write=False)
        object.__setattr__(self, '_axial', axial)
        signed_permutation = (np.all(np.sum(axial != 0, axis=0) == 1)
                              and np.all(np.sum(axial != 0, axis=1) == 1)
                              and np.all(np.isin(axial, (-1.0, 0.0, 1.0))))
        axes = tuple((int(np.flatnonzero(row)[0]), float(row[np.flatnonzero(row)[0]]))
                     for row in axial) if signed_permutation else None
        object.__setattr__(self, '_signed_axes', axes)

    def inverse(self):
        return CoordinateTransform(self.basis.T, 1.0 / self.length_scale)

    def vectors(self, values):
        vectors = np.asarray(values, dtype=np.float64)
        if vectors.ndim < 1 or vectors.shape[-1] != 3 or not np.isfinite(vectors).all():
            raise ValueError('vectors must contain finite three-component vectors')
        out = vectors @ self.basis.T * self.length_scale
        if not np.isfinite(out).all():
            raise ValueError('coordinate vector conversion overflowed')
        return out

    def rotations(self, values):
        quaternions = _quaternions(values)
        out = np.empty_like(quaternions)
        # Rotation vectors are axial: det(B) B v, with scalar part unchanged.
        # Signed permutations preserve exact legacy components and signed zero.
        # This is also the common source-axis conversion, not a target default.
        if self._signed_axes is not None:
            for row, (column, sign) in enumerate(self._signed_axes):
                out[..., row] = sign * quaternions[..., column]
        else:
            out[..., :3] = quaternions[..., :3] @ self._axial.T
        out[..., 3] = quaternions[..., 3]
        return out


def _parents(values, count):
    parents = np.asarray(values)
    if parents.shape != (count,) or (count and not np.issubdtype(parents.dtype, np.integer)):
        raise ValueError('parents must be one integer index per joint')
    if np.any(parents < -1) or np.any(parents >= count):
        raise ValueError('parent index out of range')
    parents = parents.astype(np.int64)
    state = np.zeros(count, dtype=np.uint8)
    order = []
    for start in range(count):
        index, path = start, []
        while index != -1 and state[index] == 0:
            state[index] = 1
            path.append(index)
            index = int(parents[index])
        if index != -1 and state[index] == 1:
            raise ValueError('parent graph contains a cycle')
        for index in reversed(path):
            state[index] = 2
            order.append(index)
    return parents, order


def _output_dtype(value):
    dtype = np.dtype(value)
    if dtype not in (np.dtype(np.float32), np.dtype(np.float64)):
        raise ValueError('rotation output dtype must be float32 or float64')
    return dtype


def world_to_local_rotations(world_quaternions, parents, *, dtype=np.float64):
    """Decompose explicit absolute world rotations: local = parent^-1 world.

    Roots have parent -1. Parent indices may be unsorted; no absent parent or
    topology is inferred. Output is absolute local, not rest-relative delta.
    A caller declaring identity canonical rest can label it a delta separately.
    """
    world = _quaternions(world_quaternions)
    if world.ndim != 2:
        raise ValueError('world rotations must have shape (joints, 4)')
    count = len(world)
    parents, _ = _parents(parents, count)
    dtype = _output_dtype(dtype)
    out = np.zeros((count, 4), dtype=dtype)
    if not count:
        return out
    rotations = Rotation.from_quat(world.astype(np.float64))
    locals_ = np.empty(count, dtype=object)
    for index, parent in enumerate(parents):
        locals_[index] = rotations[index] if parent == -1 else rotations[parent].inv() * rotations[index]
    # Keep the second normalization and cast order of the original consumer.
    result = Rotation.from_quat(np.stack([rotation.as_quat() for rotation in locals_]))
    out[:] = result.as_quat().astype(dtype)
    return out


def local_to_world_rotations(local_quaternions, parents, *, dtype=np.float64):
    """Rebuild absolute worlds from absolute locals on an explicit parent graph."""
    local = _quaternions(local_quaternions)
    if local.ndim != 2:
        raise ValueError('local rotations must have shape (joints, 4)')
    count = len(local)
    parents, order = _parents(parents, count)
    dtype = _output_dtype(dtype)
    out = np.zeros((count, 4), dtype=dtype)
    if not count:
        return out
    rotations = Rotation.from_quat(local)
    worlds = [None] * count
    for index in order:
        parent = parents[index]
        worlds[index] = rotations[index] if parent == -1 else worlds[parent] * rotations[index]
    out[:] = np.stack([rotation.as_quat() for rotation in worlds]).astype(dtype)
    return out
