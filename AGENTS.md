# KineTexture staging source instructions

This is a standalone source-only repository on local branch `main`. Its 80-file
reviewed payload comes from the source commit recorded in STAGING_SOURCE_HASHES.json.
Read STAGING.md before changing the staged package. Build with MSVC on Windows;
keep builds, SDKs, test output and temporary assets under ignored build paths or
outside this repository. Use the supplied synthetic tests rather than game files.

The original release_receipt.json and reviewed_preview_manifest.json are retained
verbatim. They bind historical reviewed bytes; do not silently refresh their
hashes after editing source. The four-document follow-up is explicitly recorded
in docs/SDK_FIRST_USE_20261101.md and the separate maintained_docs_manifest_20261101.json
under tools/kinetexture_release/. Use the explicit manifest command in STAGING.md
for the maintained checkout. Staging metadata is listed separately and grants
no new distribution rights. Preserve all license and provenance notices.

This source snapshot does not require sibling development repositories, links,
game installation, model weights, real clips or Python numerical packages for
native build/consumption. Python's standard library is used by development tests
and the source-manifest checker. No deployment or remote publishing is performed
by the build commands. Keep local changes reviewable with explicit Git commits.
