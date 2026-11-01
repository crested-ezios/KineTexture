# KineTexture Clean-1 Python candidate

KineTexture represents, evaluates and retargets explicit performance channels
across source formats and target characters. This package provides scalar
tracks, coordinate/bone-frame primitives and independent two-bone IK. No game,
Unity runtime, Loader, source model or target profile is required. NumPy/SciPy
are its numerical dependencies; the existing `kinetexture-vmd` standard-library
package retains its separate installation contract.

See [channel semantics](../CHANNEL_SEMANTICS.md). There is no
new file format, engine lifecycle, playback transport or full retarget pipeline.

`kinetexture.scalar.ScalarTrack` requires `Timebase`, `ChannelSemantics` and
`Provenance`. Keys are strictly increasing clock ticks; `sample(seconds)` maps
seconds through the declared rate and origin. `sample_ticks(ticks)` evaluates
in those clock coordinates directly. Linear interpolation holds endpoints;
intervals containing a missing key remain missing. An empty track is missing,
not an authored zero. Input arrays are copied and made read-only. Values are
not clamped to a facial-weight range. Nonfinite input/output or unrepresentable
time spans fail; missing values may contain unavailable sentinels.

`kinetexture.retarget` provides four-axis `align_bone_frames`, reversible
`CoordinateTransform` including reflections/length scaling, separate absolute
and delta rotation rebasing/composition, and world/local rotation conversion
using explicit parent indices. `kinetexture.ik.solve_two_bone` consumes explicit
root/goal/segment vectors and world rotation hints and returns actual FK joint
and end positions, pole, reachability and residual. It does not infer contact,
source scale, character axes or root placement.

Gaku's old function signatures are thin adapters into these modules. They keep
their target tables, name mapping, duplicate-key and absent-channel policies.
There is one implementation per Python primitive, not another solver copied
into the game converter. C++ scalar sampling is the runtime implementation;
cross-language numerical fixtures verify the corresponding scalar contract.

The wheel is a local date snapshot candidate. The older public allowlist does
not approve these new files or dependencies automatically.
