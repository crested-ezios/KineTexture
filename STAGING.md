# Standalone KineTexture source staging

This local source-only snapshot contains the exact 80 reviewed payload files,
the original release receipt, and a copy of the frozen review manifest. The
source review is [included here](docs/SOURCE_PROVENANCE_REVIEW_20261101.md).
No game files, motions, music, model weights, render caches or built DLLs were
copied. The repository has no remote configured by this staging operation.

`STAGING_SOURCE_HASHES.json` records each original payload hash, the original
receipt/manifest identities and the hashes of the staging-only metadata files.
The 80-file source baseline is `29b2387f3b608b76457157b9cc706bf81a7029ed`;
the frozen manifest SHA256 is
`68eaba23328a475e3d1577f2f243e1c8935440e2d0810021f3fd10e5ac0bccff`.
The original receipt SHA256 is
`f8b00961c05df372aa27b37eb2498d5f70df3595fe01712f8078a82f0a169de2`.
The current staging commit is a new repository checkpoint, rather than a rewrite
of that reviewed source identity.

The copied manifest is intentionally outside the original 80-file payload at
`tools/kinetexture_release/reviewed_preview_manifest.json`. The original receipt
describes the exact exported package; this Git repository additionally contains
Git metadata and the explicitly recorded staging files. Validate its frozen
source projection from this repository root with:

```powershell
python tools/kinetexture_release/prepare_preview.py --check
```

That read-only command needs no neighboring development checkout. Do not apply
the exact-output verifier to the whole Git root, which has deliberate additional
metadata; the source checker verifies the explicitly reviewed payload instead.
The complete source preview can be independently exported to a new ignored build
directory using the included tool and unchanged copied manifest.

For native build/tests, use a Visual Studio x64 developer shell with MSVC,
CMake 3.20+, Ninja and Python available:

```powershell
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/native
ctest --test-dir build/native --output-on-failure
$prefix = Join-Path (Get-Location) 'build/sdk'
cmake --install build/native --prefix "$prefix"
cmake -S tests/consumer -B build/consumer -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$prefix"
cmake --build build/consumer
ctest --test-dir build/consumer --output-on-failure
cmake -S examples/two_bone_ik -B build/two_bone_ik -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$prefix"
cmake --build build/two_bone_ik
ctest --test-dir build/two_bone_ik --output-on-failure
```

The default build enables the development CLI targets and nine CTest entries;
installed-package consumption is also checked in a relocated prefix. A native
core application needs the installed headers/package, not Python research tools.
See the unchanged README and channel semantics for API contracts and limits.

`.gitattributes` disables text conversion to preserve the reviewed raw bytes;
only this new repository's local `core.autocrlf=false` is set. Build output is
ignored. Public release, remote setup and asset rights remain separate from this
local staging operation. No build result establishes in-game visual fidelity.
