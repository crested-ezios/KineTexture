# KineTexture migration and native plan

Date: 2026-11-01 (UTC+08:00). Produced by: CDX-G6.

KineTexture is the current name of the former MotionWeave project. Its scope is
source-independent performance semantics, evaluation and retarget primitives.
Game-specific profiles, Unity/Loader integration, facial policy and playback
ownership remain in consumer adapters. No new motion distribution format is added.

## Name migration

Current C++ headers/namespace, CMake package/target, Python research packages,
VMD/release/GVHMR tools and consumer references use `KineTexture`/`kinetexture`.
The private GitHub repository is `crested-ezios/KineTexture`; visibility is unchanged.
GakuMotion pins the renamed core under `deps/KineTexture`. smpl2gkclip and the
current MMD workflow resolve the new package and active source layout.

Git history, archived experiments, freeze receipts, protected hashes and real
motion assets retain their historical identity. A renamed current Python source
tree is a new source identity; old frozen-pass receipts do not certify its hashes.
The preserved source-evaluator bundle is the historical reproducibility source.
No algorithm, tolerance, frozen Candidate B source or old oracle was changed to
make a migration comparison pass.

## Native work and remaining gates

| Work | State after this iteration | Next gate |
| --- | --- | --- |
| Float playback math, dense sampling, FK, dense scalar channels | C++ already used by GakuMotion; namespace migrated | Consumer regression and live loading remain separate |
| Coordinate transforms, bone frames, absolute/delta rebase, hierarchy rotation decomposition | New dependency-free C++ double APIs | Cross-source/target integration; no implicit character policy |
| Analytical two-bone IK | New C++ API, explicit axes/poles and actual FK residual | Target-character baking is a separate adapter port |
| In-game VMD/body/facial/eye/timebase import | Existing C# NativeAOT path made the only product backend | Full source corpus and live acceptance are not implied |
| Independent camera import | New NativeAOT camera ABI and atomic sidecar update | Live loading and camera visual acceptance |
| Existing-track editing | C# product port in GakuMotion, evaluated separately | Atomicity and retained-metadata regression |
| Source-bound target-leg geometry bake | Not ported; runtime rejects before changing assets | Port profile binding, pole/contact policy and compare real target FK |
| Irregular scalar tracks | Python research implementation remains | C++ sparse-tick API preserving missing versus authored zero |
| GVHMR/HaMeR inference, hand fusion/temporal cleanup | Python research only; derived outputs remain opt-in | A product-time consumer would require a native port first |
| VMD diagnostic CLI and release exporter | Python development/research tooling | Not part of the game runtime; native standalone CLI is later work |
| Reverse gkclip offline inspection | Native `gkclip2vmd` / `gkclip2smpl` development targets with explicit frame profiles | Built-in presets are approximate; matching target geometry, contact and visual acceptance remain separate |

Runtime code must not invoke Python or silently fall back to it. Unsupported
target geometry is reported explicitly and existing clips remain playable.
Native-only does not mean all research capabilities have been ported or visually
accepted. Port new functionality before admitting it into a product path.

## Verification

MSVC Release builds passed core/clean1/native CTest (3/3). The native suite performs
1,723 checks: analytic expected geometry, reflection/basis rules, unsorted/cyclic
parents, explicit delta composition, reachability, degenerate/180-degree cases,
failure atomicity, rigid equivariance and no ordinary heap allocations during
evaluation. An independent installed-header consumer passed the same native suite.

The research-only differential script compares 384 deterministic records with the
unchanged Python numerical implementations: 128 IK, 128 retarget and 128 hierarchy
records. Maximum differences were 2.78e-15 for vectors, 1.19e-14 radians for
rotations and 1.99e-15 for scalar results (acceptance bound 2e-9). These checks do
not establish live-game visual quality or target contact correctness.

The renamed VMD/release wheels were installed in a fresh isolated venv without
network/package-index access. Fixture generation, inspection, sampling, export,
receipt verification and rejection of an unlisted file passed. Numerical research
package installation is checked separately from the dependency-free CLI wheels.

Reproducible core commands executed in this checkout:

```powershell
cmake --build build/native_20261101 --config Release
ctest --test-dir build/native_20261101 -C Release --output-on-failure
```

The Python differential runner is a development oracle, never a runtime dependency.
Detailed consumer build, deployment hashes and rollback evidence are recorded in
GakuMotion's current handoff; deployment, process loading and user visual
acceptance are separate gates.


Pre-rename HandTrack NPZ and compatible-local VMD profiles remain readable via
explicit legacy-schema adapters. HandTrack upgrades only in memory and records
the original schema in provenance; subsequent saves use the new schema. HaMeR
preparation reads the legacy receipt filename only when the new name is absent.
Old assets, model/network hashes and temporal-processing guards stay intact.
Four real raw/processed hand tracks (4,640 frames) were read with unchanged arrays
and file hashes; historical prepared inference provenance was also checked without
rewriting its receipt. This is research compatibility, not a new runtime fallback.
