[简体中文](README.zh-CN.md) | **English**

# KineTexture

This header-only C++20 core contains scalar math, dense pose and scalar-track
sampling, explicit channel/skeleton data and forward kinematics. It has no game, graphics, audio, platform,
model-weight or third-party-library dependency. Native APIs also provide explicit
coordinate conversion, bone-frame retarget primitives and analytical two-bone IK.
It does not infer character profiles, contact correction or source-file semantics.
The API is experimental; no stable binary ABI is provided.

[Channel semantics](CHANNEL_SEMANTICS.md) include absolute/delta,
missing/explicit zero, timebase and provenance. The independent
[Python research package](python/README.md) supplies
irregular ScalarTrack, coordinate/bone-frame retarget primitives and TwoBoneIK;
Gaku's research converters call these implementations. KineTexture's scope is performance
representation/evaluation/retargeting across sources and character targets.

## Start here

KineTexture is an SDK and a set of offline tools. It is not a GakuLoader plugin;
there is no KineTexture DLL to install into the game or its `plugins/` directory.
GakuMotion packages carry the matching core headers at build time.

This repository provides source code. No prebuilt SDK or CLI download is linked
from this page; the commands below create an installable SDK from a checkout or
source archive. No adjacent GakuMotion repository or game installation is needed.

- Integrate the C++ library: [build and install the SDK](#build-and-verification),
  then [consume it from your own project](#use-an-installed-sdk-in-your-own-project).
- Inspect a `.gkclip` offline: [build and use the native CLI tools](tools/gkclip_export/README.md).
- Explore the algorithms in Python: see the separate [research package](python/README.md).

The SDK contains headers, a CMake package, documentation and license notices.
When the optional CLI targets are enabled, installation also includes their
executables in `bin/`. Keep the SDK prefix intact when copying it to another
project or machine; the installed-package test checks relocation explicitly.

## Math and dense sampling

`Vector3` and `Quaternion` retain the scalar float layouts and arithmetic of the
existing runtime: xyzw Hamilton quaternions, active `q.Rotate(v)`, and
`parent * local` composition. `Inverse()` is the conjugate of a **unit** rotation.
`Normalized()` preserves the original zero fallback; validation, rather than the
low-level math method, rejects nonfinite inputs.

`DenseClipView` borrows frame-major arrays. Call `ValidateDenseClip()` at ingestion
to check shapes, finite roots, and unit rotations (norm tolerance 0.001).
`SampleDensePose()` takes seconds and exact-sized caller output storage. It uses
root Lerp and shortest-path Slerp with the original scalar expression order.
Negative time holds the first frame; past-end holds the final frame. NaN/Inf time
and malformed shapes fail without changing caller outputs. Large finite times
clamp before integer conversion. Input/output arrays must not overlap, input
storage must remain alive, and callers must not mutate validated data concurrently.

## Skeletons and FK

`SkeletonDescriptor` separates external `BoneId` from vector index and declares
parents, rest local rotations and offsets, units and coordinate basis. Names are
optional; nonempty names and external IDs must be unique. Bone axes must be finite,
nonzero and nonparallel. They describe the profile; the core never infers bone frames. Explicit
frame alignment is available through `AlignBoneFrames`. Zero rest offsets are allowed for coincident helper joints.
Each root uses parent `-1`; multiple roots require explicit permission.

`PoseSchema` binds a frame to the descriptor identity/revision and its numeric
basis. Lengths are meters, timestamps are seconds, quaternion storage is xyzw.
Only schema revision 1 is currently supported. Coordinate declarations never
request conversion: all data must already use the declared numeric basis.

`FkPlan::Prepare()` validates a descriptor/schema and computes a parent-first
evaluation order even when input joints are unsorted. Failure preserves the prior
plan. The default prepare budget is 65,536 joints. `PrepareContext()` allocates the
scratch for one consumer. A successful re-prepare invalidates old contexts; plans
are shared read-only during evaluation, with a separate context per concurrent
consumer. Re-prepare/moving a plan must not race with evaluation.

For a joint `i`, the rotation formulas are:

- LocalAbsolute: `world[i] = world[parent] * pose[i]`.
- RestRelative: `world[i] = world[parent] * rest[i] * pose[i]`.
- WorldAbsolute: `world[i] = pose[i]`.

The root parent's rotation is identity. Positions use
`position[i] = position[parent] + world[parent].Rotate(offset[i])`;
the root parent's position is `frame.root_translation`. `offset[i]` is the
descriptor's rest offset unless `local_translations` is supplied, which **replaces**
the rest offset. The rotation representation never changes translation semantics.
The same root translation is applied once per declared root. No external actor
orientation is injected; output remains in the schema's declared model/world frame.

An empty valid mask means all joints are available. Otherwise values must be 0/1.
A valid joint with a missing parent fails. Missing joints produce valid=0 with
zero-position/identity sentinels, which must not be treated as measured identity
poses. Invalid slots may carry unavailable input values; only valid slots are read.
FK errors leave caller output untouched. After successful prepare, sampling and FK
use no heap allocation; they do not discover objects, parse names or access files.

## Build and verification

Native development targets `gkclip2vmd` and `gkclip2smpl` export existing
canonical-local clips for offline inspection. They provide explicit frame
profiles and approximate opt-in MMD/SMPL/SMPL-X presets, with missing/omitted
channels recorded in a report. See [usage and limits](tools/gkclip_export/README.md).
They are separate adapters; `KineTexture::Core` remains independent of legacy
game formats and model assets.

Use CMake 3.20+, Ninja and a C++20 compiler. On Windows, open an **x64 Native
Tools Command Prompt** or **Developer PowerShell** for Visual Studio with the
Desktop development with C++ and C++ CMake tools components installed. Confirm
`cl`, `cmake`, `ctest` and `ninja` are available in that shell. No Python, game
installation or model assets are needed to build or consume the core.

From the repository root, these PowerShell commands build the core checks and
install an SDK. The export adapters are optional and disabled in this minimal
core example; their existing targets can be enabled separately.

```powershell
$installPrefix = Join-Path (Get-Location) 'build/install'
cmake -S . -B build/core -G Ninja -DCMAKE_BUILD_TYPE=Release -DKINETEXTURE_BUILD_GKCLIP_TOOLS=OFF
cmake --build build/core
ctest --test-dir build/core --output-on-failure
cmake --install build/core --prefix "$installPrefix"
```

For an independent application, use `find_package(KineTexture CONFIG REQUIRED)`
and `target_link_libraries(your_target PRIVATE KineTexture::Core)`. Pass the
**installation prefix**, not the repository's `src` directory, to CMake:

```powershell
cmake -S tests/consumer -B build/consumer -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$installPrefix"
cmake --build build/consumer
ctest --test-dir build/consumer --output-on-failure
```

`tests/consumer` is a standalone CMake project and includes only installed
headers. The root CTest suite also installs into an isolated prefix, relocates
that prefix to a path containing spaces, and configures/builds/runs this consumer
and the [two-bone IK/FK example](examples/two_bone_ik/README.md) independently.
Its artifacts stay under the build tree for diagnosis. A clean prefix contains
`include/KineTexture/`, the exported CMake package under `lib/cmake/KineTexture/`
(or the platform's configured lib directory), and documentation/license files
under `share/KineTexture/`. Source builds and Python research packages are not
required by an application using that prefix.

Pin a reviewed commit or tag for an integration and build its headers, package
metadata and documentation together. The experimental API has no compatibility
promise across arbitrary revisions; the CMake package currently exports a target
without a semantic-version compatibility check.

The installed `README.md` is generated from this page. Links to repository-only
material point to GitHub; `CHANNEL_SEMANTICS.md` is included beside it. Run
`python tools/check_docs.py` to check maintained links and README consistency;
run `python tools/check_docs.py --write-installed-readme` after editing this page.
This documentation tool is a development check, not a native runtime dependency.

## Use an installed SDK in your own project

An SDK consumer does not need this source checkout. In a new project directory,
create these two files. `KineTexture::Core` supplies its C++20 requirement and
installed include directory through the exported CMake target.

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(KineTextureExample LANGUAGES CXX)
find_package(KineTexture CONFIG REQUIRED)
add_executable(kinetexture_example main.cpp)
target_link_libraries(kinetexture_example PRIVATE KineTexture::Core)
```

`main.cpp`:

```cpp
#include <KineTexture/KineTexture.hpp>
#include <cmath>

int main()
{
    const auto q = KineTexture::Quaternion::FromEulerDegreesXYZ(0, 0, 90);
    const auto direction = q.Rotate({1, 0, 0});
    return std::abs(direction.x) < 1e-5f && std::abs(direction.y - 1) < 1e-5f ? 0 : 1;
}
```

From that new project directory, use the absolute `$installPrefix` created by
the installation step above. If you received an SDK separately, set that variable
to the directory containing its `include`, `lib` and `share` directories:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$installPrefix"
cmake --build build
.\build\kinetexture_example.exe
```

The example exits with code zero after checking a 90-degree rotation. For
explicit two-bone IK, pole hints and reconstructed FK, follow the standalone
[two-bone example](examples/two_bone_ik/README.md).

The tests use two self-authored skeletons with different nonidentity rest rotations,
bone axes, lengths and coordinate declarations; one uses unsorted parents and sparse
external IDs. Expected positions are analytic right-angle examples, independent of
the tested FK. Tests also cover interleaved contexts, explicit representation
differences, masks, failure atomicity, malformed data and ordinary heap-allocation
counts. These are offline core checks. Game-consumer equivalence, publication
provenance, other operating systems and visual acceptance require separate evidence.


## Native numerical APIs (2026-11-01)

`Retarget.hpp` adds `Vector3d`, `Quaterniond`, `CoordinateTransform`,
`AlignBoneFrames`, absolute/delta rebase, explicit delta composition and a prepared
`RotationHierarchy`. The existing float playback types and expression order stay
unchanged. Coordinate declarations do not trigger implicit conversion; callers
prepare an orthogonal basis and positive length scale explicitly. Hierarchy
preparation validates unsorted parents and cycles; evaluation uses caller storage
without allocating and preserves outputs on error.

`TwoBoneIK.hpp` supplies `SolveTwoBone`. Arbitrary local segment axes, unequal
lengths, explicit/previous poles and roll hints are supported. Results contain the
actual FK joint/end positions, radial reachability and endpoint residual. There is
no target-character, foot-contact or persistent temporal policy in the solver.

MSVC native checks, installed consumers and a research-only Python differential
oracle are recorded in [the migration plan](docs/NATIVE_MIGRATION_20261101.md).
Python remains available for research, model inference and release tooling;
applications should use the C++/C# runtime paths.

The optional research oracle uses NumPy/SciPy and is deliberately separate from
the default native build/CTest path. In an environment with the research package
and those dependencies already installed, its entry is
`python tests/compare_native_research.py --help`; do not add those dependencies to
an application's core runtime. See the migration plan for the recorded numerical
comparison and its limits. The Windows/MSVC workflow checks the core suites,
relocated installed consumers and maintained documentation; it does not certify
game visuals or target-character contact.

The former project name was MotionWeave. Active packages, headers, namespaces,
CMake targets and CLIs now use KineTexture/kinetexture. Historical Git commits,
freeze receipts, archived experiments and source hashes retain their original
identity. These records are provenance, not alternate current runtime APIs.
