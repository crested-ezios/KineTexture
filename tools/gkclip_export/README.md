# Native gkclip development export targets

`gkclip2vmd` and `gkclip2smpl` are C++20 command-line targets for offline
inspection. Neither starts the game, calls Python, loads Unity, downloads
weights nor links a model renderer. They are compatibility adapters outside
`KineTexture::Core`. Exported VMD/NPZ can be opened in existing external tools.

## Build

From the repository root in a Visual Studio x64 developer shell with CMake and
Ninja available:

```powershell
cmake -S . -B build/cli -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/cli
```

The executables are in `build/cli/`. The targets
build by default; `-DKINETEXTURE_BUILD_GKCLIP_TOOLS=OFF` disables them. CMake
installation also installs the executables under `bin/`. Core consumers do
not acquire a dependency on these adapters.

Check the executable entry points without providing a motion file:

```powershell
.\build\cli\gkclip2vmd.exe --help
.\build\cli\gkclip2smpl.exe --help
```

CTest uses generated synthetic clips, so its CLI checks need no private sample
or external model. Python's standard library is required for that test only.
The export examples below require your own source clip; `motion.gkclip` is an
input-path example, not a file shipped in this repository.

## Export

The following invocations use a local source clip and new output paths:

```powershell
.\build\cli\gkclip2vmd.exe motion.gkclip --preset canonical-mmd --body-only --output review.vmd
.\build\cli\gkclip2smpl.exe motion.gkclip --preset canonical-smpl --body-only --output review_smpl.npz
.\build\cli\gkclip2smpl.exe motion.gkclip --preset canonical-smplx --body-only --neutral-shape --output review_smplx.npz
```

Each export also creates `OUTPUT.report.json` with the full profile
configuration, source/output timebase, available joint mask, missing target
HIDs, omitted source HIDs, omitted camera and every omitted morph name.
Existing output, report and profile paths are refused. Failures roll back
files created by the operation. Input clips are read-only.

The named presets are **approximate orientation previews** with self-authored
cardinal bone frames. They contain no extracted character geometry. They
explicitly correct leg/foot and mirrored arm frames; they are not an exact
inverse of an arbitrary source converter. They require gkclip v2–5; v1 needs
an explicit profile matching its earlier frame convention. MMD uses the
declared `diag(-1,1,-1)` basis and `0.083` meters per VMD position unit;
SMPL/SMPL-X use `diag(-1,1,1)` and meters. These are opt-in named conventions,
not facts stored in the input file.

The MMD preset declares a 52-bone hierarchy including `上半身3`. A model with
another hierarchy (including a common 51-bone variant) needs an edited profile;
an ignored intermediate track also changes the orientation of its descendants.

Actual-mesh review corrected the MMD leg/foot hinge convention in preset
`canonical-mmd-approx/2`; SMPL presets are unchanged. Model-specific profiles
and real-model render comparisons are external research artifacts, not bundled
samples. Use an [explicit frame profile](#explicit-frame-profiles) for another rig.
Declared motion projection, timing and roots passed their checks; source-pose
equivalence and contact remain unaccepted. First-version MMD exports should
not be used as accepted geometry references.

By default, any omitted source rotation, camera or morph causes an error.
`--body-only` explicitly allows those omissions and records them, including
authored-zero morph tracks. VMD missing tracks are then absent, not synthesized
as measured identity. Standard leg/toe IK is disabled at frame zero so it does
not override the baked FK preview; custom model IK/append behavior still needs
appropriate rig configuration. No original IK controller or helper tracks are
recovered. v5 head-space eye channels cannot be mapped as local body rotations;
omit them explicitly until a measured eye-frame adapter is supplied.

VMD uses a 30 FPS timebase and dense baked keys with linear interpolation.
Higher-rate gkclip is sampled in seconds, using root lerp and local quaternion
slerp. SMPL preserves source FPS unless `--fps NUMBER` is supplied. The last
source timestamp is `(frame_count-1)/fps`, independently of the legacy duration
header. When output grids differ, the final grid key holds the source endpoint;
the report gives both last timestamps. High-frequency motion may require the
source-rate SMPL export for analysis.

`gkclip2smpl` writes an uncompressed ZIP/NPY NPZ readable with NumPy
`allow_pickle=False`, containing:

| Key | Shape / meaning |
| --- | --- |
| `poses` | `(F,72)` SMPL or `(F,66)` SMPL-X body, axis-angle radians |
| `global_orient` | `(F,3)` root axis-angle |
| `body_pose` | `(F,69)` SMPL or `(F,63)` SMPL-X body |
| `transl` | `(F,3)` converted clip-relative displacement plus profile offset, meters |
| `fps`, `mocap_framerate` | scalar output sample rate |
| `time_seconds` | `(F,)` output grid timestamps |
| `joint_valid` | `(24,)` or `(22,)` availability mask |
| `model_type` | byte-string scalar `smpl` or `smplx` |
| `betas` | optional `(1,N)` explicitly supplied shape |

SMPL's two terminal hand joints have no direct source body counterpart and are
neutral placeholders with `joint_valid=0`. Finger/face parameters are not
reconstructed. Unknown shape is omitted; `--neutral-shape` explicitly supplies
ten zero coefficients for preview, or `--betas 0.1,-0.2,...` accepts a known
shape. The existing SMPL-X offline renderer needs `canonical-smplx` and an
explicit shape choice; its model weights remain an external dependency. A
renderer ignoring the availability mask will display neutral placeholders.

## Explicit frame profiles

Write an editable preset and then supply it explicitly:

```powershell
.\build\cli\gkclip2vmd.exe --preset canonical-mmd --write-profile review.profile
.\build\cli\gkclip2vmd.exe motion.gkclip --profile review.profile --body-only --output review_bound.vmd
```

Profiles are development adapter configuration, not a new motion distribution
format. The header is `kinetexture-gk-export-profile 1`. Fields are mandatory,
unique and bounded to 64 KiB. `#` starts a whole-line comment. Joint records
are in parent-first target order:

```text
kinetexture-gk-export-profile 1
id "self-authored-two-bone-example"
target vmd
source_versions 1 5
approximate 0
basis 1 0 0 0 1 0 0 0 1
meters_per_unit 1
root_offset 0 0 0
root_track 726f6f74
# joint source_hid parent_index name_hex alignment_xyzw rest_local_xyzw
joint 0 -1 68697073 0 0 0 1 0 0 0 1
joint 7 0 7370696e65 0 0 0 1 0 0 0 1
```

`basis` is a row-major orthogonal matrix (reflections allowed). Quaternions
are unit xyzw. Names are hex-encoded CP932 bytes for VMD, ASCII identifiers
for SMPL; `-` is an empty root-track field for SMPL. Parent `-1` declares a
root. Source HID `-1` declares an unavailable target slot. For SMPL, joint
order and topology must follow the selected model's standard 24/22 body order.
Root offsets use output position units. The VMD translation track is separate
from rotational roots. Model-specific IK controllers are not inferred.

For each available source joint, reconstruction first follows the existing
gkclip HID parent chain. If its direct parent is absent, the legacy file
defines a separate chain root. Mapping a retained source descendant to a
different target parent then uses world rotations:

```text
target_world[j] = basis(source_world[source_hid]) * alignment[j]
target_local[j] = inverse(target_world[parent]) * target_world[j]
output_delta[j] = inverse(rest_local[j]) * target_local[j]
```

For an exact inverse of an import that used `source_world * K`, alignment is
the basis-converted `inverse(K)`. It must be derived from the same source/target
bone frames, including roll; direction alone cannot determine it. A gkclip
does not store K, bone lengths or shape. It cannot restore absolute stage
origin, source-specific helper/append behavior or a calibrated pelvis pivot.
`root_scale` is provenance only: root positions are already meters and are
never multiplied by it again.

These exports can check channel content, orientation projection, timing and
motion discontinuities offline. Character geometry, contact, skinning,
game facial policy, physics and in-game visual quality require their own
matching rig/model evidence. A successful export does not certify them.

## VMD interpolation update

The 2026-11-04 writer fills all four redundant VMD interpolation blocks with
linear controls. This corrects Z translation and rotation between authored keys
in readers that use those blocks. Key values, timestamps and the output format
are unchanged. Regenerate affected VMD exports with the updated CLI.

## Verification

Build all tests and run CTest:

```powershell
cmake --build build/cli
ctest --test-dir build/cli --output-on-failure
```

Native tests independently inspect VMD fields and NPZ/NPY records and compare
frame transforms to an analytic matrix oracle. If Python is available, CTest
also runs a standard-library-only end-to-end CLI check. Python is a test
dependency only. Real-clip smoke checks are recorded separately from synthetic
numeric acceptance and visual acceptance.
