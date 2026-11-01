"""Installed MW consumer using only self-authored numeric fixtures.

Run with the installed package from any working directory. This exercises the
middleware contract, not a source-format adapter or a game/engine integration.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from scipy.spatial.transform import Rotation

from kinetexture import (
    ChannelSemantics, Provenance, Representation, ScalarTrack, Timebase, ValueType,
)
from kinetexture.ik import solve_two_bone
from kinetexture.retarget import (
    CoordinateTransform, align_bone_frames, compose_rotation_delta,
    rebase_absolute_rotation, rebase_rotation_delta,
)


def check_rotation(actual, expected, tolerance=2e-12):
    error = float((Rotation.from_quat(actual).inv() * Rotation.from_quat(expected)).magnitude())
    if error > tolerance:
        raise AssertionError(f'rotation error {error} exceeds {tolerance}')
    return error


def run():
    fixture_payload = {'source-A': {'ticks': [0, 12, 24, 36], 'values': [0, 1, None, .25]},
                       'source-B': {'ticks': [0, 60], 'values': [0, 0]}}
    digests = {name: hashlib.sha256(json.dumps(value, sort_keys=True).encode('utf8')).hexdigest()
               for name, value in fixture_payload.items()}
    absolute = ChannelSemantics('expression.amount', ValueType.SCALAR,
                                Representation.ABSOLUTE, 'unitless', 'expression')
    provenance_a = Provenance('synthetic-source-A', digests['source-A'],
                              'expression.amount', 'self-authored fixture')
    source_a = ScalarTrack([0, 12, 24, 36], [0, 1, np.nan, .25],
                           valid=[1, 1, 0, 1], timebase=Timebase(24, 3),
                           semantics=absolute, provenance=provenance_a)
    sampled = source_a.sample([2.5, 3, 3.25, 3.5, 3.75, 4, 4.25, 4.5, 5])
    np.testing.assert_array_equal(sampled.valid, [1, 1, 1, 1, 0, 0, 0, 1, 1])
    np.testing.assert_array_equal(sampled.values, [0, 0, .5, 1, 0, 0, 0, .25, .25])
    if source_a.provenance != provenance_a or source_a.timebase.seconds(12) != 3.5:
        raise AssertionError('source provenance or clock mapping was lost')

    delta = ChannelSemantics('translation.adjustment', ValueType.SCALAR,
                             Representation.DELTA, 'meters', 'model', 'synthetic-reference-B')
    provenance_b = Provenance('synthetic-source-B', digests['source-B'],
                              'translation.adjustment', 'self-authored fixture')
    delta_zero = ScalarTrack([0, 60], [0, 0], timebase=Timebase(60, -2),
                             semantics=delta, provenance=provenance_b)
    absent = ScalarTrack([], [], timebase=Timebase(60, -2),
                         semantics=delta, provenance=provenance_b)
    authored = delta_zero.sample([-2, -1.5, -1])
    missing = absent.sample([-2, -1.5, -1])
    np.testing.assert_array_equal(authored.values, [0, 0, 0])
    np.testing.assert_array_equal(missing.values, [0, 0, 0])
    np.testing.assert_array_equal(authored.valid, [1, 1, 1])
    np.testing.assert_array_equal(missing.valid, [0, 0, 0])
    try:
        ChannelSemantics('unbound.delta', ValueType.SCALAR, Representation.DELTA,
                         'unitless', 'expression')
    except ValueError:
        pass
    else:
        raise AssertionError('a delta without a reference was accepted')

    # Independently authored right-angle chain: unequal lengths .8 and .6.
    # Inverting known orientations produces arbitrary segment-local axes.
    root = np.array([.3, .4, -.2])
    upper_hint = Rotation.from_euler('xyz', [23, -37, 41], degrees=True)
    lower_hint = Rotation.from_euler('xyz', [-19, 33, 71], degrees=True)
    upper = upper_hint.inv().apply([.8, 0, 0])
    lower = lower_hint.inv().apply([0, .6, 0])
    goal = root + [.8, .6, 0]
    solved = solve_two_bone(root, goal, upper, lower, upper_hint, lower_hint)
    np.testing.assert_allclose(solved.joint_position, root + [.8, 0, 0], atol=2e-12)
    np.testing.assert_allclose(solved.end_position, goal, atol=2e-12)
    fk_joint = root + solved.upper_world.apply(upper)
    fk_end = fk_joint + solved.lower_world.apply(lower)
    np.testing.assert_allclose(fk_joint, solved.joint_position, atol=2e-12)
    np.testing.assert_allclose(fk_end, solved.end_position, atol=2e-12)
    if not solved.reachable or solved.residual > 2e-12:
        raise AssertionError('the analytic reachable chain failed')
    unreachable = solve_two_bone(root, root + [3, 0, 0], upper, lower,
                                 upper_hint, lower_hint)
    np.testing.assert_allclose(unreachable.end_position, root + [1.4, 0, 0], atol=2e-12)
    np.testing.assert_allclose(unreachable.residual, 1.6, atol=2e-12)
    np.testing.assert_allclose(root + unreachable.upper_world.apply(upper)
                               + unreachable.lower_world.apply(lower),
                               unreachable.end_position, atol=2e-12)
    if unreachable.reachable:
        raise AssertionError('the unreachable chain was marked reachable')

    identity = np.array([0., 0., 0., 1.])
    pairs = [([17, -31, 12], [-9, 40, 66]), ([-43, 7, 29], [32, -24, -51])]
    rotation_errors = []
    for index, (source_angles, target_angles) in enumerate(pairs):
        source_frame = Rotation.from_euler('xyz', source_angles, degrees=True).as_matrix()
        target_frame = Rotation.from_euler('xyz', target_angles, degrees=True).as_matrix()
        frame = align_bone_frames(source_frame[:, 0], source_frame[:, 1],
                                  target_frame[:, 0], target_frame[:, 1])
        np.testing.assert_allclose(Rotation.from_quat(frame).as_matrix(),
                                   source_frame @ target_frame.T, atol=2e-12)
        reference = Rotation.from_euler('xyz', [21 + index, -13, 39], degrees=True).as_quat()
        change = Rotation.from_euler('xyz', [-12, 31 + index, 47], degrees=True).as_quat()
        posed = compose_rotation_delta(reference, change, space='local')
        rebased_pose = rebase_absolute_rotation(posed, frame)
        rebased_reference = rebase_absolute_rotation(reference, frame)
        rebased_delta = rebase_rotation_delta(change, frame)
        composed = compose_rotation_delta(rebased_reference, rebased_delta, space='local')
        rotation_errors.append(check_rotation(composed, rebased_pose))
        rotation_errors.append(check_rotation(rebase_rotation_delta(identity, frame), identity))
        rotation_errors.append(check_rotation(rebase_absolute_rotation(identity, frame), frame))
        if (Rotation.from_quat(frame)).magnitude() < .1:
            raise AssertionError('fixture accidentally hid absolute/delta identity distinction')

        # Nontrivial reflected numeric bases; explicit scale acts on lengths.
        basis = (Rotation.from_euler('xyz', [11 + index, -22, 37], degrees=True).as_matrix()
                 @ np.diag([-1., 1., 1.]))
        conversion = CoordinateTransform(basis, (.01, 1.25)[index])
        converted = conversion.rotations(posed)
        np.testing.assert_allclose(Rotation.from_quat(converted).as_matrix(),
                                   basis @ Rotation.from_quat(posed).as_matrix() @ basis.T, atol=2e-12)
        rotation_errors.append(check_rotation(conversion.inverse().rotations(converted), posed))
        vector = np.array([2., -4., 7.])
        np.testing.assert_allclose(conversion.inverse().vectors(conversion.vectors(vector)),
                                   vector, atol=2e-12)
        np.testing.assert_allclose(conversion.vectors(Rotation.from_quat(posed).apply(vector)),
                                   Rotation.from_quat(converted).apply(conversion.vectors(vector)), atol=2e-12)

    return {'status': 'PASS', 'scope': 'installed MW; self-authored numeric fixtures',
            'synthetic_source_target_pairs': len(pairs),
            'scalar_samples': int(sampled.values.size),
            'scalar_missing_samples': int(np.count_nonzero(~sampled.valid)),
            'explicit_zero_and_delta_zero_preserved': True,
            'ik_reachable_residual': solved.residual,
            'ik_unreachable_residual': unreachable.residual,
            'maximum_retarget_rotation_error_radians': max(rotation_errors),
            'source_digests': digests}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--cpp-scalar-trace', type=Path)
    args = parser.parse_args()
    report = run()
    if args.cpp_scalar_trace is not None:
        import struct
        checks = 0
        for row in args.cpp_scalar_trace.read_text(encoding='ascii').splitlines():
            mode, clock, value, valid = row.split(',')
            track = ScalarTrack([0, 1, 2], [0, 2, -2], timebase=Timebase(2, 10),
                semantics=ChannelSemantics('custom:strength', ValueType.SCALAR,
                    Representation.ABSOLUTE, 'unitless', 'semantic'),
                provenance=Provenance('synthetic:installed-consumer'),
                valid=[1, 0, 1] if int(mode) else None)
            sample = track.sample(float.fromhex(clock))
            assert bool(sample.valid) == bool(int(valid))
            assert struct.pack('<f', float(sample.values)) == struct.pack('<f', float.fromhex(value))
            checks += 1
        assert checks == 16
        report['cpp_python_scalar_samples_byte_equal'] = checks
    payload = json.dumps(report, sort_keys=True, ensure_ascii=False, allow_nan=False)
    if args.report is not None:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(payload + '\n', encoding='utf8')
    print(payload)


if __name__ == '__main__':
    main()
