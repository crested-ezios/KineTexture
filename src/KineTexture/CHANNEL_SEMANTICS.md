# Clean-1 channel semantics

KineTexture represents and evaluates performance semantics across source formats
and target characters. Clean-1 implements scalar sampling, explicit frame/basis
primitives and independent two-bone IK. These are in-memory contracts; no new
`.mwclip` or other disk format is introduced.

| Contract | Meaning |
| --- | --- |
| Channel identity | Caller supplies a nonempty semantic name, value type, unit and space. A target morph index, HID, filename or prefix never defines universal meaning. |
| Absolute | A scalar value or absolute orientation in the declared domain/frame. Identity is an available absolute orientation, not a missing sample. |
| Delta | A change relative to an explicit `reference_id`. Scalar zero and rotation identity mean an authored no-change command. They do not release ownership or ask for a default. |
| Availability | `valid=0` means unavailable; `valid=1` with zero/identity is explicit data. Empty track means missing. No sampler replaces missing with authored zero/identity or interpolates across a missing key. Output zero for an unavailable scalar is only a sentinel. |
| Timebase | Positive finite sample rate in Hz and finite origin in seconds. `seconds = origin + tick / rate`. Irregular keys use ticks in that declared clock, not positional array indices. Caller aligns channels in seconds. No FPS is inferred from VMD or another extension. |
| Provenance | `source_id` and operation are required; source channel/digest are optional declared evidence. A missing digest is unknown, not an approval. Derived profiles and source model identities remain separate records. |

No layer infers that normalized facial weights, degrees, metres or sample clocks
are interchangeable. Clean-1 scalar values have no intrinsic range clamp. The
Gaku adapter continues its existing normalized-weight/clamp and alias policies.
Its morph key sampler receives the converter/refresh caller's explicit source
clock (the legacy direct-helper default remains 30 Hz),
resolves duplicate keys with the last authored key and applies its existing
absent-track-to-zero behavior at the adapter boundary.

## Sampling and precision

C++ `ScalarTrack` owns one dense float32 channel with semantics/timebase/provenance.
`DenseScalarTracksView` is a borrowed frame-major numeric view for prepared
adapters; it does not own or infer the corresponding column descriptors. Call
validation at ingestion and keep borrowed storage alive/immutable during use.
The default exact-key evaluation reads that key directly. Gaku explicitly uses
`ScalarKeyEvaluation::LinearExpression` for its unmasked legacy weight stream,
preserving the original float expression and signed-zero behavior. No range
clamp or new weight policy is added. Validation errors preserve output buffers;
sample-time validation/ordinary sampling allocates no heap memory for the view.

Python `ScalarTrack` owns irregular float64 keys/values and immutable copies of
its input arrays. `sample(seconds)` uses the declared timebase;
`sample_ticks(ticks)` exposes numeric evaluation in that same clock. Duplicate
ticks are rejected by the generic layer. Both samplers hold endpoints, reject
NaN/Inf queries and unrepresentable interpolation results, and require both
keys for an interval. An exact key needs only that key. Finite far-out times
hold endpoints before integer/time-coordinate overflow can change the result.
Float32 and float64 paths share semantics, not a promise of bitwise equality
for every possible interpolation; the installed cross-language fixture checks
representable examples, while each Gaku language path has its own byte oracle.

## Rotation and retarget primitives

Quaternions use Hamilton-active xyzw, `world = parent * local`, and explicit
parent indices. Retarget functions consume available numeric rotations; missing
inputs must be handled before calling them, never filled with identity.

- `align_bone_frames` requires source and target primary **and** hinge axes.
  Its `K` maps target-frame coordinates into source-frame coordinates.
- Absolute frame rebase is `R_target = R_source * K`.
  A source-frame delta rebases as `D_target = inverse(K) * D_source * K`.
  Applying a delta requires an explicit reference: local `R * D`, parent `D * R`.
- Numeric basis conversion uses an explicit orthonormal `B` and positive length
  scale: vectors `scale * B * v`, rotations `B * R * inverse(B)`. Reflections
  use the axial-vector quaternion mapping `q.xyz -> det(B) * B * q.xyz`.
- World/local decomposition consumes an explicit acyclic parent graph; it does
  not infer humanoid topology. Absolute locals become canonical deltas only
  when an adapter explicitly declares identity canonical rest.
- TwoBoneIK consumes explicit segment vectors and world rotation hints, reports
  actual FK endpoints/reachability/residual, and keeps `previous_pole` state in
  the caller. Reset that state on discontinuities. It infers neither contact
  nor character scale and does not manipulate a game object.

Existing `PoseSchema.rotations` still declares one representation for a whole
pose; scalar semantics do not silently override it. Gaku gkclip v5 body locals
and Head-space eye deltas remain distinct adapter semantics. A neutral consumer
must separate or explicitly resolve these before declaring a uniform FK pose.
Clean-1 does not change Eye/Facial/Tooth product policies or complete a new v5 IR.
