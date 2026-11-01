"""Synthetic-only tests of fixed review records and local preview boundaries."""

from __future__ import annotations

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / 'prepare_preview.py'
spec = importlib.util.spec_from_file_location('prepare_preview', SCRIPT)
preview = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preview)


class ReviewedPreviewTests(unittest.TestCase):
    def setUp(self):
        # Only these self-authored temporary bytes are marked approved in tests.
        # No actual repository source receives an approval from this generator.
        (preview.ROOT / 'build').mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix='preview-manifest-fixture-',
                                                dir=preview.ROOT / 'build')
        self.folder = Path(self.temp.name)
        self.source = self.folder / 'source'
        self.source.mkdir()
        files = []
        for name in preview.PREVIEW_FILES:
            data = ('Self-authored synthetic fixture: ' + name + '\n').encode('utf-8')
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            files.append({
                'source': name, 'destination': name,
                'sha256': hashlib.sha256(data).hexdigest(),
                'owner': 'Self-authored test fixture generator',
                'source_type': 'generated', 'license': 'Apache-2.0',
                'upstream_url': None, 'upstream_revision': None,
                'notice': 'Synthetic text; no source code, assets or actual provenance approval',
                'review_status': 'approved', 'reviewer': 'Synthetic fixture author',
                'evidence': ['test_preview_manifest.py:setUp creates these exact synthetic bytes'],
            })
        self.record = {
            'schema_version': 1, 'preview_policy': preview.POLICY,
            'snapshot': {
                'source_head': '1' * 40,
                'reviewer': 'Synthetic fixture author',
                'review_evidence': ['Self-authored test generator only; not an actual source review'],
                'public_distribution_authorized': False,
            },
            'files': files,
        }
        self.manifest = self.folder / 'synthetic-reviewed-manifest.json'
        self.output = self.source / 'build' / 'new source preview'
        self.save()

    def tearDown(self):
        self.temp.cleanup()

    def save(self):
        self.manifest.write_text(json.dumps(self.record, indent=2) + '\n', encoding='utf-8')

    def reject(self, match):
        self.save()
        with self.assertRaisesRegex(preview.exporter.ReleaseError, match):
            preview.prepare_preview(self.source, self.manifest, self.output)
        self.assertFalse(self.output.exists())

    def test_check_is_read_only_and_does_not_refresh_approval(self):
        before = self.manifest.read_bytes()
        result = preview.prepare_preview(self.source, self.manifest)
        self.assertEqual(result['file_count'], len(preview.PREVIEW_FILES))
        self.assertEqual(result['status'], 'reviewed-source-verified')
        self.assertEqual(before, self.manifest.read_bytes())
        self.assertFalse((self.source / 'build').exists())

    def test_export_embeds_exact_external_manifest_without_self_reference(self):
        before = self.manifest.read_bytes()
        result = preview.prepare_preview(self.source, self.manifest, self.output)
        self.assertEqual(result['status'], 'verified')
        self.assertEqual(result['manifest_sha256'], hashlib.sha256(before).hexdigest())
        receipt = json.loads((self.output / 'release_receipt.json').read_text(encoding='utf-8'))
        self.assertEqual(receipt['manifest'], self.record)
        self.assertEqual(before, self.manifest.read_bytes())
        self.assertFalse((self.output / self.manifest.name).exists())
        self.assertFalse(list(self.output.glob('*_sbom.json')))
        self.assertEqual(preview.exporter.verify_release(self.output)['file_count'], len(preview.PREVIEW_FILES))

    def test_changed_source_bytes_require_new_review(self):
        (self.source / 'CMakeLists.txt').write_text('unreviewed source change\n', encoding='utf-8')
        self.reject('SHA256 mismatch')
        self.assertEqual(self.record['files'][0]['sha256'],
                         json.loads(self.manifest.read_text())['files'][0]['sha256'])

    def test_missing_manifest_entry_is_rejected(self):
        self.record['files'].pop()
        self.reject('completeness mismatch')

    def test_extra_asset_entry_is_rejected(self):
        extra = dict(self.record['files'][0], source='assets/private.gkclip',
                     destination='assets/private.gkclip')
        self.record['files'].append(extra)
        self.reject('completeness mismatch')

    def test_remapped_destination_is_rejected(self):
        self.record['files'][0]['destination'] = 'other/CMakeLists.txt'
        self.reject('preserves every reviewed file path')

    def test_missing_reviewer_is_rejected(self):
        del self.record['files'][0]['reviewer']
        self.reject('reviewer requires')

    def test_unknown_reviewer_is_rejected(self):
        self.record['files'][0]['reviewer'] = 'UNKNOWN'
        self.reject('reviewer requires')

    def test_missing_file_evidence_is_rejected(self):
        self.record['files'][0]['evidence'] = []
        self.reject('nonempty evidence')

    def test_pending_evidence_is_rejected(self):
        self.record['files'][0]['evidence'] = ['pending']
        self.reject('explicit review record')

    def test_pending_approval_is_not_automatically_promoted(self):
        self.record['files'][0]['review_status'] = 'pending'
        self.reject('unapproved')
        self.assertEqual(json.loads(self.manifest.read_text())['files'][0]['review_status'], 'pending')

    def test_snapshot_needs_fixed_reviewed_source_commit(self):
        self.record['snapshot']['source_head'] = 'main'
        self.reject('reviewed source commit')

    def test_snapshot_needs_review_evidence(self):
        self.record['snapshot']['review_evidence'] = []
        self.reject('nonempty evidence')

    def test_cannot_claim_public_distribution_authorization(self):
        self.record['snapshot']['public_distribution_authorized'] = True
        self.reject('must not authorize public')

    def test_wrong_preview_policy_is_rejected(self):
        self.record['preview_policy'] = 'unknown'
        self.reject('preview_policy')

    def test_output_outside_build_is_rejected(self):
        self.output = self.folder / 'outside'
        self.reject('below this checkout build')

    def test_existing_output_is_preserved(self):
        self.output.mkdir(parents=True)
        marker = self.output / 'user-owned.txt'
        marker.write_text('keep')
        with self.assertRaisesRegex(preview.exporter.ReleaseError, 'already exists'):
            preview.prepare_preview(self.source, self.manifest, self.output)
        self.assertEqual(marker.read_text(), 'keep')
        self.assertEqual([p.name for p in self.output.iterdir()], ['user-owned.txt'])

    def test_missing_source_file_is_rejected(self):
        (self.source / 'CMakeLists.txt').unlink()
        with self.assertRaises(FileNotFoundError):
            preview.prepare_preview(self.source, self.manifest, self.output)
        self.assertFalse(self.output.exists())

    def test_source_hash_change_after_preflight_still_fails_before_copy(self):
        original = preview.exporter.export_release

        def mutate(*args, **kwargs):
            (self.source / 'CMakeLists.txt').write_text('changed after check')
            return original(*args, **kwargs)

        with patch.object(preview.exporter, 'export_release', side_effect=mutate):
            with self.assertRaisesRegex(preview.exporter.ReleaseError, 'SHA256 mismatch'):
                preview.prepare_preview(self.source, self.manifest, self.output)
        self.assertFalse(self.output.exists())

    def test_manifest_change_after_preflight_is_hash_bound(self):
        original = preview.exporter.export_release

        def mutate(*args, **kwargs):
            self.record['files'].pop()
            self.save()
            return original(*args, **kwargs)

        with patch.object(preview.exporter, 'export_release', side_effect=mutate):
            with self.assertRaisesRegex(preview.exporter.ReleaseError, 'manifest changed after preflight'):
                preview.prepare_preview(self.source, self.manifest, self.output)
        self.assertFalse(self.output.exists())

    def test_duplicate_json_key_is_rejected(self):
        raw = self.manifest.read_text(encoding='utf-8')
        self.manifest.write_text(raw.replace('"schema_version": 1,',
                                            '"schema_version": 1, "schema_version": 1,'), encoding='utf-8')
        with self.assertRaisesRegex(preview.exporter.ReleaseError, 'duplicate JSON key'):
            preview.prepare_preview(self.source, self.manifest, self.output)
        self.assertFalse(self.output.exists())

    def test_hardlinked_fixture_is_rejected(self):
        try:
            os.link(self.source / 'CMakeLists.txt', self.folder / 'linked-copy.txt')
        except OSError as exc:
            self.skipTest(f'local filesystem cannot create test hardlink: {exc}')
        with self.assertRaisesRegex(preview.exporter.ReleaseError, 'hard-linked'):
            preview.prepare_preview(self.source, self.manifest, self.output)
        self.assertFalse(self.output.exists())


if __name__ == '__main__':
    unittest.main()
