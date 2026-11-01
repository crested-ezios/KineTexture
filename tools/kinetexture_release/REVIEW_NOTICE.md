# Complete source preview review scope

This notice explains the local preview workflow. It does not approve any file or
change the historical root or installed SDK `NOTICE` files.

The external reviewed manifest is the file-level record for one exact payload.
It identifies hashes, contributors/owners, source categories, licenses, notices,
reviewers and evidence. A list of required build files is not an ownership or
license decision. Only a completed, SHA-bound review may set an entry approved.
Source changes need a new review of those bytes, not an automatic hash refresh.

The companion [source review](../../docs/SOURCE_PROVENANCE_REVIEW_20261101.md)
records the newer native-adapter and onboarding review and preserves the full
GakuMotion MIT notice required by the first-party compatibility adaptation.
Its per-file license treatment is retained in the frozen manifest; the root
Apache notice does not erase that MIT attribution.

The complete preview is limited to explicitly named source code, project files,
synthetic test generators, SDK examples, maintained guides and release tooling.
It includes the development adapters `gkclip2vmd` and `gkclip2smpl`; their legacy
format knowledge remains separate from the dependency-free native core. The
research numerical differential still has its separately declared NumPy/SciPy
dependencies. Those dependencies do not enter native core execution.

No Git history, game assets, captured pose arrays, real motion/music, extracted
profiles, PMX/SMPL geometry, trained weights, render caches or runtime binaries
are included. Historical real-input research reports and their local artifacts
are outside this source preview. The adapter guide retains historical references
to `docs/GKCLIP_OFFLINE_RENDER_REVIEW_20261101.md` and
`docs/GKCLIP_EXPORT_VALIDATION_20261101.md` in the original source checkout.
Their source baseline for this follow-up is
`86f69ebd47597f75e31d679fb06b0bffdabfca99`; those paths identify external evidence
rather than files promised inside this preview. Do not fetch or copy associated
models, clips, weights or render products to fill these references.

The manifest freezes the exact bytes copied from the reviewed checkout. Git blob
hashes are recorded separately where checkout line endings differ; a clean Git
status does not imply identical raw bytes. The exporter does not normalize line
endings or refresh an approved hash. A different checkout representation must
receive a new exact-byte review before it can be exported with this policy.
The delivered source snapshot itself retains its exact reviewed bytes and can
be built without Git or the original checkout.

The frozen manifest remains outside the payload and is embedded in
`release_receipt.json`. This permits review of the release script's exact bytes
without including the manifest's own hash recursively. Later commits adding the
manifest or external review records do not silently change the approved payload.

Local review, copying and build tests do not publish anything and do not certify
game/client compatibility or visual fidelity. Existing GakuMotion game acceptance
remains valid for its original scope. Public release, repository visibility and
distribution channels remain separately authorized decisions.
