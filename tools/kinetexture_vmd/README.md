# KineTexture.VMD local preview

Python 3.10+; no runtime dependencies. The packaging version identifies a date
snapshot and does not promise a stable API. Public distribution is pending the
project's file-level provenance/license review.
The tested wheel-build versions are pinned in `build-requirements.txt`; a
preprovisioned environment can build with `pip wheel --no-index --no-deps
--no-build-isolation .`. Only Windows CPython 3.12.4 has been tested this round;
the Python minimum is a source compatibility declaration, not a runner matrix.

This package reads real VMD 0002 bone, morph, camera, light, shadow and property
sections with explicit input budgets. `inspect` reports source facts without
assuming FPS, a target rig, a game installation or a character.

`sample` is a narrow **compatible-local** path. Both profiles are mandatory:
source joints declare VMD tracks, parents, meter-valued rest transforms, FPS,
VMD-position unit scale and coordinate basis; target joints explicitly map those
source joints and supply the same topology, rest transforms and basis. The
source must already contain baked local channels. There is no MMD IK, append,
helper-bone collapse, basis conversion or arbitrary-skeleton retargeter here.
Morph/camera/light/shadow/IK/property tracks and unmapped source tracks are
rejected by this exporter. `inspect` still parses those sections.

The legacy target adapter is isolated in `gkclip_compat.py`. It verifies explicit
HIDs and the existing consumer topology, identity rest rotations, all tracks
present and no non-root local translation before writing existing gkclip v2.
The binary does not carry rest geometry: retain the `.profile.json` sidecar.
The JSON sampling option is diagnostic/test interchange, not a new distribution
format. The old game converter and motion corpus are unchanged.

After installing the wheel, `kinetexture-vmd make-fixture --output demo` creates
self-authored synthetic VMD and explicit source/target profiles. Then use
`kinetexture-vmd inspect demo/synthetic.vmd` and
`kinetexture-vmd sample demo/synthetic.vmd --source-profile demo/source.json --target-profile demo/target.json --output demo/motion.gkclip`.
The three-frame fixture uses root translation 0..2 meters, a 0..90 degree root
Z rotation and a 0..60 degree child X rotation, with a 0.5 meter child offset.
It is deliberately authored in one matching coordinate basis; it is not a
claim that ordinary MMD motion can be loaded on an arbitrary target unchanged.

Interpolation uses destination-key VMD Bezier controls (20-step bisection),
normalized shortest-arc quaternion SLERP, endpoint holding and explicit FPS.
Missing tracks remain invalid in diagnostic output and are rejected for gkclip.
Output paths must be new; failures do not overwrite existing artifacts.

Source provenance: the binary layout and legacy topology were checked against
this repository's `vmd_parser.py`, `gkclip_format.py` and `GkMotionData.hpp` at
source baseline `c4a9e6588eff730e93efe8cfcc1b9be68cd7b2b0`; the Bezier numeric
method follows its `bezier.py`. No third-party Octo/Noesis code, game profiles,
character constants, real motion/music, extracted assets or trained models are
included. These factual source records do not grant licenses to unknown inputs.
