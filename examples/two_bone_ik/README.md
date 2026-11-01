# Two-bone IK and explicit FK

This standalone application consumes an installed `KineTexture::Core`. Follow
the [SDK installation steps](../../README.md#build-and-verification), then run
these PowerShell commands from the repository root in the same developer shell:

```powershell
$installPrefix = Join-Path (Get-Location) 'build/install'
cmake -S examples/two_bone_ik -B build/two_bone_ik -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$installPrefix"
cmake --build build/two_bone_ik
ctest --test-dir build/two_bone_ik --output-on-failure
```

The two one-meter local +X segments target `(1, 1, 0)` with an explicit +Y pole.
The example prints the returned joint, reconstructs the endpoint through FK using
the returned **world** rotations, and checks the endpoint against the analytic
target. It also demonstrates a goal beyond the chain's two-meter reach: the
solver reports `reachable=false`, the clamped endpoint and a nonzero residual.

Supply your own units, coordinate frame, segment geometry, rotation hints and
pole policy. The example does not discover a skeleton or provide foot contact,
character calibration or temporal state. It needs no Python, renderer, game
files or model assets. Root CTest runs this same project against a relocated SDK.
