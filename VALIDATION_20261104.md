# KineTexture maintenance validation

The maintained 80-file source projection uses
`tools/kinetexture_release/maintained_source_manifest_20261104.json`.
Its `snapshot.source_head` identifies the commit containing those exact payload
bytes. The later staging freeze commit also records the manifest and selection
metadata; these two identities are recorded separately in the validation receipt.

Run the manifest checker described in [STAGING.md](STAGING.md), then build the
exported source with MSVC x64, CMake, Ninja, C++20 and the static runtime:

```powershell
cmake -S <exported-source> -B <new-build> -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DKINETEXTURE_BUILD_GKCLIP_TOOLS=ON -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build <new-build>
ctest --test-dir <new-build> --output-on-failure
```

The nine CTest entries cover the core, native math, VMD writer, CLI output and
relocated installed consumer. Documentation, release-tool regression tests,
exact source export, static-runtime CLI imports and extracted SDK consumption
are checked separately. No game, motion, model or numerical Python dependency
is needed for these synthetic checks.

Actual command logs, results, source/manifest hashes and the SDK ZIP identity
are kept outside this source repository under
`build/staging_refresh_20261104/KineTexture/` in the parent workspace. The final
receipt binds the clean staging freeze commit and verifies all 80 exported
payload files against that commit. Any reused build is identified with its
original source payload commit and explicit byte-equivalence evidence.
