# Source provenance review: native export and SDK onboarding

Reviewed base: `86f69ebd47597f75e31d679fb06b0bffdabfca99`. Review date: 2026-11-01.

This explicit source review supports a local developer-preview allowlist. It does not publish code or grant public-release owner approval. The original core/Python/VMD source baseline is reviewed separately by the parent task. Every row below was read individually and compared with Git introduction/change history and available authoring records. Build success and commit author names were not treated as source approval.

## Decision

These 24 files are eligible at their exact source hashes for the local preview allowlist, with the existing LICENSE/NOTICE and this companion review retained. No model arrays, fitted rest geometry, SMPL weights, real motion sample or external renderer implementation is embedded in these reviewed bytes. This is not blanket approval for other repository files or future changes.

The standard-library SMPL writer implements stored ZIP/NPY numeric records, CRC32 and axis-angle serialization. The VMD writer implements explicit fixed-width binary fields and authored linear controls. Joint names, parent indices, file magic and cardinal axes are interoperability/schema conventions. They do not supply model weights or rights to unrelated external assets.

`Clip.hpp` is an authored compatibility adaptation informed by GakuMotion's MIT-licensed HID parent and versioned clip-format contracts; retain the complete source notice below. `Presets.hpp` uses ideal cardinal frames and an explicit approximate `0.083` unit convention. It does not copy the measured TARGET_OFFSET_DIR vectors, real rest joints or model geometry present in other local converter work. In `86f69eb`, real-mesh testing exposed a sign error, but the source change is a cardinal hinge sign, preset revision and synthetic matrix regression; the fitted YYB profile stays outside this selection.

## Evidence

- **E1/E2**: `c6745860fd233dfe6937a57b282c2d9967c1ce7e` adds the new source/tests. Implementation task `01a196e4-e295-7732-82e7-c4b64a9ea748`, turn `01a196e4-e760-7033-bbce-92103abae33e`, records explicit Clip/Profiles/CLI additions (`exec-fd5b9b09-76fd-4618-a9cc-47667188aeae`, `exec-366ec1cd-a9f9-4540-a041-6b3402a3eab6`, `exec-4d881fdb-d875-4d56-8c40-99564ce31e83`).
- **E3**: VMD writer/test task `01a196e5-8e94-7642-82bd-4454aa769815`, turn `01a196e5-8edf-7fb1-aad4-86a07f2d5ae9`, additions `exec-0ce48e57-a763-44a1-b72e-1dae1ff7f079` and `exec-10229741-6c1b-4743-ab74-6ecf68c5bed9`. Local references were the existing first-party parser and synthetic fixture.
- **E4**: SMPL writer/test task `01a196e5-c376-7c91-a105-510592848e23`, turn `01a196e5-c3c2-74b0-8bdb-e9b518158d0d`, additions `exec-84ab2a34-750d-4f1c-986a-950ad35d5ee9` and `exec-6abd11d6-c62c-4a54-b5a2-e4b449053ec8`. Inspected source contains no geometry loader or body-model data.
- **E5/E6/E7**: First-party format/topology references and MIT source notice are pinned below.
- **E8**: `smpl2gkclip` `convert.py` lines 109-135 record standard SMPL-X parents/mapping, attributing the parent order to GVHMR `postprocess.py`. That converter, its measured offsets at lines 158+, its models and rendering dependencies are not redistributed or relicensed by this review.
- **E9**: `86f69eb` changes only signed cardinal hinge conventions and adds an independent ideal neutral-forward/inverse matrix test to source. No measured YYB profile is compiled into the preset.
- **E10**: `d3f9c16` adds independently consumed SDK validation, a one-meter two-bone analytic example, source-document checks and CI.
- **E11**: The installed README exactly reproduces `tools/check_docs.py::installed_readme()` over the reviewed root README. No external generation input is used.

### Pinned compatibility references

| Repository / file | Last change | SHA256 |
| --- | --- | --- |
| GakuMotion `src/GkMotionData.hpp` | `72e6daf430c9a76003934c1924a9569afc28d7c1` | `f3903465d35b6efcc9c659b43eff1cf8fa31468b69704ba3b83c674d3e23f979` |
| GakuMotion `tools/vmd2gkmotion/gkclip_format.py` | `774107160dd2d393c7a428c1d660374a2d37d5f1` | `d56d280653f885fc4ebb70d9775dde340a7db6993fc4aa3cbe23b147ae26dc99` |
| GakuMotion `LICENSE` | `099e511908825301a5c249d37c59dcf061c12e41` | `563f83844d4a86122e6d71ad774f9a8a8ece53391e927f6bb49233dafab0c2e1` |
| smpl2gkclip `convert.py` | `4ef661835abeb95d2195c6bc8f1fdbaa79c02a27` | `9035ef504067dc003a8ca6b676101a5b0cb856ab306ea1e8a7d27f8f5be02bd5` |

## Exact source files

`original` identifies authored source, documentation or an explicitly described first-party adaptation; it does not deny schema references. `generated` identifies mechanically derived text. No external implementation is relabeled original merely because it was committed locally.

SHA256 below freezes actual source bytes for copying. The Git blob hash and actual worktree hash are separately recorded in the machine review. Root README has a clean Git status but a checkout-only CRLF difference; its normalized text matches the fixed base exactly. None of the four installed SDK README/channel/license/notice files was changed by this review.

| File | Classification / evidence | Introduced | Last changed | Source SHA256 |
| --- | --- | --- | --- | --- |
| `tools/gkclip_export/main.cpp` | original; E1, E2 | `c674586` | `c674586` | `5e40d957d2942d3d6cc0e6dd7ff9f297f15a588e6ada6387e97400ee2061b470` |
| `tools/gkclip_export/Clip.hpp` | original; E1, E2, E5, E6, E7, E8 | `c674586` | `c674586` | `26195890e374e32c0b8b58ce6b10021c5e5021b7f3ca729cc0c6fdc2eb79f877` |
| `tools/gkclip_export/Profiles.hpp` | original; E1, E2 | `c674586` | `c674586` | `24ef9ea9b36244d3121462f262ad49efd1c436df2135aaefbaa9891778aa1dc6` |
| `tools/gkclip_export/Presets.hpp` | original; E1, E2, E8, E9 | `c674586` | `86f69eb` | `0bdc362c891a441f797cda97f74e507f3942afcfbe1904a4fc6b8f49a00dc740` |
| `tools/gkclip_export/VmdWriter.hpp` | original; E1, E3 | `c674586` | `c674586` | `9df09a00e22ad8db65b673aa62ce966ea5c43bc59ff24aaddbb4b1664a8c5d19` |
| `tools/gkclip_export/SmplWriter.hpp` | original; E1, E4 | `c674586` | `c674586` | `4b4a1584867bc83afc7467cd5d33a4562e4a982ba18bcf980cfd57e363e4fa8f` |
| `tests/test_gkclip_vmd.cpp` | original; E1, E3 | `c674586` | `c674586` | `04a8868af0206c48040a36cc5cff1cef0e5e53604227e5967635bf5fa3909705` |
| `tests/test_smpl_export.cpp` | original; E1, E4 | `c674586` | `c674586` | `cc4a3038bd8f33938b6df86b0cba9e0209edf1e8823ae52f6267e0482734c737` |
| `tests/test_gkclip_adapter.cpp` | original; E1, E2 | `c674586` | `c674586` | `907b6154ca2ba077d92bd88811582969a06136b085fcc33b79eca74162f42266` |
| `tests/test_gkclip_presets.cpp` | original; E1, E2, E8, E9 | `c674586` | `86f69eb` | `b8941627995c382b132af24e7eb9fae76a22ca2685f2966aaec68634d199a9a0` |
| `tests/test_gkclip_cli.py` | original; E1, E2 | `c674586` | `c674586` | `77f26ee8eb3f5dcbd347d55cc6704dedd94938a35a9be74d1e36847336b22e08` |
| `examples/two_bone_ik/CMakeLists.txt` | original; E10 | `d3f9c16` | `d3f9c16` | `23d65aaa5513b458a93a173a078b884ee58d09e389383d7c668f4062f7ed4a17` |
| `examples/two_bone_ik/main.cpp` | original; E10 | `d3f9c16` | `d3f9c16` | `d9e161115e8e8e270976bfacd9b200e8dfddcf9ec76b72c95077fbf4a8e356d6` |
| `examples/two_bone_ik/README.md` | original; E10 | `d3f9c16` | `d3f9c16` | `f60483270a81d27fdb2943a9ca1dd5b0fd5fbac374b1275f28470a2f9b7fec26` |
| `tests/verify_installed_consumer.cmake` | original; E10 | `d3f9c16` | `d3f9c16` | `d43e23f2d15e81f7bba35b2b983c28c4690e4b56fdb2bbe5e54528697814fe85` |
| `tools/check_docs.py` | original; E10 | `d3f9c16` | `d3f9c16` | `a26dfc0653e2502ffb15da8ee7f8863d6c63224ab6f6fe617d977835f7400287` |
| `CMakeLists.txt` | original; E1, E10 | `c27b4df` | `d3f9c16` | `b45fb9ca3140efbbfd4208c4f3d738470f4c88d92b14729b7a92cad8ac16269b` |
| `.github/workflows/windows-native.yml` | original; E10 | `d3f9c16` | `d3f9c16` | `a713e148c96f81648571a9874b403363057ab49f0caf39c5ab36a56508ee4318` |
| `README.md` | original; E1, E10 | `c27b4df` | `d3f9c16` | `a8cd1d2c855e5979c8f9084a411908d59378683adc54c14f99f7157c49d9a7aa` |
| `python/README.md` | original; E1, E10 | `c27b4df` | `d3f9c16` | `7b6062e04ca586422c0863c1bfa7db90e479162ffc8b4537a33d800847e587fc` |
| `src/KineTexture/README.md` | generated; E10, E11 | `c27b4df` | `d3f9c16` | `3300a24ae2c74447d915fe9a47f8c6ee3478bc11d5bc8695459f1d0136977992` |
| `tools/gkclip_export/README.md` | original; E1, E2, E9 | `c674586` | `86f69eb` | `b594f2067e0ba7f7940d9fbedec82f09c1fe5846d9232b32eee904bdad43c8fc` |
| `docs/NATIVE_MIGRATION_20261101.md` | original; E1, E10 | `2947266` | `c674586` | `f9accd2e01406877ca8e015351b62b2c7e2f3a7945960b6556737e9ea14fae33` |
| `docs/DEVELOPER_ONBOARDING_20261101.md` | original; E10 | `d3f9c16` | `d3f9c16` | `8ac8b818463d1bde2641b95d51d78d1031fa12efe75acb400d8e7adeee5b1187` |

## Exclusions and remaining boundaries

Exclude `docs/GKCLIP_EXPORT_VALIDATION_20261101.md` and `docs/GKCLIP_OFFLINE_RENDER_REVIEW_20261101.md` from this source-preview selection. These research reports are not native build dependencies. Their real-model/motion references, fitted profiles, outputs and ignored build caches are outside this review and are not bundled. A historical research link in a tool README does not extend the allowlist to its target or assets.

No uncertain embedded implementation or asset source remains in this scoped file set. This review does not verify unmodified historical research claims, hosted CI execution or public-release owner approval. Changed source bytes require another explicit reviewed hash.

## Historical native, Python, VMD and release source review

Separate reviewers completed full-file source reading and local object/license
comparisons for 35 core/Python/test/document files and 25 historical tooling
files (two tooling `.gitignore` files are not selected for this preview).
Overlapping files are not counted as separate payloads. The final exact
80-file selection and each decision are recorded in the frozen manifest;
the old 59-file receipt's automatically populated approval was not inherited.

The native Math/Sampling/Pose files have a specific Apache-licensed
`src/MotionWeave/` origin at the legacy `ef0e91e13b367089130ea304b7c7a9a1b0441804`
snapshot, an unchanged initial import at
`c27b4dfd029f20bbab55baf1b405ea91baf53376`, and the reviewed namespace migration
at `2947266a10076dec2f6bd05ec6ab07d432026b43`. The legacy Vector3/Quaternion
expressions were also compared with the actual pre-extraction source. New
native Retarget/TwoBoneIK source at `2947266` was read in full and contains
parameterized math, not a model or captured-data payload.

The five Python modules have affirmative package-specific LICENSE/NOTICE
evidence at legacy commit `848aa0a268ef96719e2906fb889eb0024c90389e` in
`tools/motionweave_python/`. The absence of a root LICENSE at the historical
`ef0e91` snapshot is not filled by retroactively assuming the newer GakuMotion
root MIT grant. The specific native and Python Apache subpackage notices are
the evidence used here. NumPy/SciPy are imported external research dependencies;
their implementations are not copied into the candidate.

The VMD/release tools trace to the legacy first addition
`09d09f2cffd1e8c56985b18f9d6879db8b8f2e58`, the `ef0e91` snapshot, and the
initial standalone import. Code comparison identified the project-name
substitution and the single explicit legacy-schema compatibility change in
`profiles.py`; the latter does not add an asset or silently alter the math.
The VMD fixture is an authored Root/Child skeleton and four mathematical keys.
The native and Python test fixtures are visible literal/formula-generated
inputs, not captured clips or model samples. No inspected transition removed
an upstream copyright/SPDX header. The release tools' new changes were reviewed
separately below rather than attributed to this historical review.

Apache LICENSE files are classified as third-party license instruments, with
their official source/version retained; the Apache Software Foundation is not
recorded as the owner of project implementation files. The native adapters'
separate MIT attribution is retained below. These findings do not assert a
complete pre-initial-commit contributor rights chain or a global originality
certification, and do not authorize public distribution.

## Follow-up review of the release tooling

The parent reviewer separately read the complete final `prepare_preview.py`,
`exporter.py` diff, new `test_preview_manifest.py`, release README and
`REVIEW_NOTICE.md`. These are first-party additions for this authorized local
preparation task. The source checker consumes a reviewer-supplied manifest,
never creates approval or refreshes an approved hash, checks the complete named
source set, and binds the manifest digest again when copying. Synthetic fixture
approval is limited to text created inside the test, not actual repository code.
Independent read-only review also checked the digest binding and approval gate.

The test result for that change is 22 cases: 21 passed and one hardlink fixture
skipped because Windows denied creating the link. That result supports the
tool's behavior; the source and license review above is a separate decision.
The final companion notices and this review text are original documentation for
the task, except for the explicitly retained upstream license text below.

## Corrected historical notice reference

The previous root, native SDK, VMD-tool and release-tool notices spelled the
historical tag as `baseline/pre-kinetexture-20261019`. That tag does not exist.
The reviewed local legacy repository resolves the actual historical tag
`baseline/pre-motionweave-20261019` to
`c4a9e6588eff730e93efe8cfcc1b9be68cd7b2b0`.

Those four notices have been corrected to the real tag and complete source
commit. Their remaining copyright, scope and distribution conditions are
unchanged. The correction does not create or rename a Git tag, grant rights to
excluded files, or relicense another repository. The corrected exact-byte
notice hash is `85a9cc31a652274a17ec800a9fbd047366f970c3e342a5af0e65fc6e32cc7d95`.
The follow-up installed SDK is checked against its final exported source
notice; the earlier fixed-86 installation evidence remains historical evidence.

## Retained GakuMotion compatibility-source notice

Preserve this notice with a local source preview containing the compatibility adaptation described above.

```text
MIT License

Copyright (c) 2026 GakuMotion Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
