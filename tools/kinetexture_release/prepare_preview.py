"""Check a frozen review manifest, then optionally copy a new local source preview.

The explicit file set is a completeness requirement, not a provenance approval.
Reviewers supply exact hashes and recorded evidence in a separate manifest.
This tool never generates approval, refreshes hashes, builds or publishes.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/kinetexture_release/src'))
from kinetexture_release import exporter

POLICY = 'kinetexture.complete-source-preview/1'
REVIEW_MANIFEST = ROOT / 'tools/kinetexture_release/reviewed_preview_manifest.json'
PREVIEW_FILES = '''CMakeLists.txt
README.md
LICENSE
NOTICE
CHANNEL_SEMANTICS.md
docs/NATIVE_MIGRATION_20261101.md
docs/SOURCE_PROVENANCE_REVIEW_20261101.md
.github/workflows/windows-native.yml
src/KineTexture/Math.hpp
src/KineTexture/Sampling.hpp
src/KineTexture/Pose.hpp
src/KineTexture/Channels.hpp
src/KineTexture/ScalarTrack.hpp
src/KineTexture/Retarget.hpp
src/KineTexture/TwoBoneIK.hpp
src/KineTexture/KineTexture.hpp
src/KineTexture/README.md
src/KineTexture/LICENSE
src/KineTexture/NOTICE
src/KineTexture/CHANNEL_SEMANTICS.md
src/KineTexture/cmake/KineTextureConfig.cmake.in
tests/test_core.cpp
tests/test_clean1.cpp
tests/test_native.cpp
tests/compare_native_research.py
tests/python_consumer.py
tests/consumer/CMakeLists.txt
tests/consumer/main.cpp
tests/verify_installed_consumer.cmake
tests/test_gkclip_vmd.cpp
tests/test_smpl_export.cpp
tests/test_gkclip_adapter.cpp
tests/test_gkclip_presets.cpp
tests/test_gkclip_cli.py
examples/two_bone_ik/CMakeLists.txt
examples/two_bone_ik/main.cpp
examples/two_bone_ik/README.md
tools/check_docs.py
tools/gkclip_export/main.cpp
tools/gkclip_export/Clip.hpp
tools/gkclip_export/Profiles.hpp
tools/gkclip_export/Presets.hpp
tools/gkclip_export/VmdWriter.hpp
tools/gkclip_export/SmplWriter.hpp
tools/gkclip_export/README.md
python/README.md
python/LICENSE
python/NOTICE
python/pyproject.toml
python/src/kinetexture/__init__.py
python/src/kinetexture/semantics.py
python/src/kinetexture/scalar.py
python/src/kinetexture/retarget.py
python/src/kinetexture/ik.py
tools/kinetexture_vmd/build-requirements.txt
tools/kinetexture_vmd/LICENSE
tools/kinetexture_vmd/NOTICE
tools/kinetexture_vmd/pyproject.toml
tools/kinetexture_vmd/README.md
tools/kinetexture_vmd/src/kinetexture_vmd/__init__.py
tools/kinetexture_vmd/src/kinetexture_vmd/__main__.py
tools/kinetexture_vmd/src/kinetexture_vmd/cli.py
tools/kinetexture_vmd/src/kinetexture_vmd/fixture.py
tools/kinetexture_vmd/src/kinetexture_vmd/gkclip_compat.py
tools/kinetexture_vmd/src/kinetexture_vmd/parser.py
tools/kinetexture_vmd/src/kinetexture_vmd/profiles.py
tools/kinetexture_vmd/src/kinetexture_vmd/sampling.py
tools/kinetexture_release/build-requirements.txt
tools/kinetexture_release/LICENSE
tools/kinetexture_release/NOTICE
tools/kinetexture_release/prepare_preview.py
tools/kinetexture_release/pyproject.toml
tools/kinetexture_release/README.md
tools/kinetexture_release/REVIEW_NOTICE.md
tools/kinetexture_release/src/kinetexture_release/__init__.py
tools/kinetexture_release/src/kinetexture_release/__main__.py
tools/kinetexture_release/src/kinetexture_release/cli.py
tools/kinetexture_release/src/kinetexture_release/exporter.py
tools/kinetexture_release/verify_clean_install.py
tools/kinetexture_release/tests/test_preview_manifest.py'''.splitlines()


def require_review_text(value, label):
    if (not isinstance(value, str) or not value.strip() or
            value.strip().casefold() in {'unknown', 'pending', 'unreviewed', '?'}):
        raise exporter.ReleaseError(f'{label} requires an explicit review record')


def require_evidence(value, label):
    if not isinstance(value, list) or not value:
        raise exporter.ReleaseError(f'{label} requires a nonempty evidence list')
    for item in value:
        require_review_text(item, label)


def validate_preview_manifest(manifest):
    """Validate scope and supplied review records without manufacturing them."""
    exporter.validate_manifest(manifest)
    if manifest.get('preview_policy') != POLICY:
        raise exporter.ReleaseError(f'preview_policy must be {POLICY}')
    snapshot = manifest['snapshot']
    if not re.fullmatch(r'[0-9a-f]{40}', str(snapshot.get('source_head'))):
        raise exporter.ReleaseError('snapshot.source_head must identify the reviewed source commit')
    require_review_text(snapshot.get('reviewer'), 'snapshot.reviewer')
    require_evidence(snapshot.get('review_evidence'), 'snapshot.review_evidence')
    if snapshot.get('public_distribution_authorized') is not False:
        raise exporter.ReleaseError('this local preview must not authorize public distribution')
    expected = set(PREVIEW_FILES)
    if len(expected) != len(PREVIEW_FILES):
        raise exporter.ReleaseError('internal preview file set contains duplicates')
    supplied = {entry['source'] for entry in manifest['files']}
    if supplied != expected:
        raise exporter.ReleaseError(
            f'preview completeness mismatch: missing={sorted(expected-supplied)}; '
            f'unexpected={sorted(supplied-expected)}')
    for entry in manifest['files']:
        if entry['source'] != entry['destination']:
            raise exporter.ReleaseError('complete source preview preserves every reviewed file path')
        require_review_text(entry.get('reviewer'), f"{entry['source']}: reviewer")
        require_evidence(entry.get('evidence'), f"{entry['source']}: evidence")
    return manifest


def check_preview(source_root, manifest_path):
    """Read-only hash gate using the exporter's same link and byte protections."""
    root = exporter._no_links(source_root)
    if not root.is_dir():
        raise exporter.ReleaseError('source root must be an existing directory')
    raw = exporter._regular(manifest_path, max_bytes=16 * 1024 * 1024)
    manifest = validate_preview_manifest(exporter._json(raw.decode('utf-8')))
    total = 0
    for entry in manifest['files']:
        path = root.joinpath(*exporter._relative(entry['source']).parts)
        data = exporter._regular(path, max_bytes=128 * 1024 * 1024 - total)
        total += len(data)
        if hashlib.sha256(data).hexdigest() != entry['sha256']:
            raise exporter.ReleaseError(f"reviewed SHA256 mismatch: {entry['source']}; obtain a new review")
    return manifest, hashlib.sha256(raw).hexdigest(), total


def prepare_preview(source_root, manifest_path, output=None):
    manifest, digest, total = check_preview(source_root, manifest_path)
    if output is None:
        return {'status': 'reviewed-source-verified', 'file_count': len(manifest['files']),
                'payload_bytes': total, 'manifest_sha256': digest}
    root = exporter._no_links(source_root)
    output = exporter._no_links(output)
    build = root / 'build'
    if not output.is_relative_to(build) or output == build:
        raise exporter.ReleaseError('preview output must be a new directory below this checkout build/')
    if output.exists():
        raise exporter.ReleaseError('preview output already exists')
    # The manifest stays outside the payload and is embedded verbatim in the
    # receipt. Its own hash is never recursively included in its file entries.
    source_manifest = exporter._no_links(manifest_path)
    if source_manifest.is_relative_to(output):
        raise exporter.ReleaseError('review manifest must remain outside the new output')
    output.parent.mkdir(parents=True, exist_ok=True)
    # export_release binds its read to these validated manifest bytes and repeats
    # every source hash check before holding the immutable payloads for copying.
    return exporter.export_release(root, manifest_path, output,
                                   expected_manifest_sha256=digest)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, default=REVIEW_MANIFEST,
                        help='Frozen reviewer-supplied manifest; never generated by this tool')
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--check', action='store_true', help='Read-only scope, review and exact-hash gate')
    mode.add_argument('--output', type=Path, help='NEW local source candidate below build/')
    args = parser.parse_args(argv)
    try:
        result = prepare_preview(ROOT, args.manifest, args.output)
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except (OSError, ValueError, TypeError, KeyError) as exc:
        print(f'preview: {exc}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
