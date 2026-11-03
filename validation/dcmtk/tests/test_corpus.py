"""Standard-library checks of the corpus enumeration; pixel oracles are in corpus_oracle_test.py."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'validation/dcmtk'))
from corpus import (BIG_ENDIAN, CODEC_FORMATS, COMPRESSED_SYNTAXES, DEFLATED, IOD_VALIDATOR_CONFLICTS, LOSSY_BOUNDS,
                    NATIVE_SYNTAXES, PIXEL_FORMATS, STORAGE_SOPS, SYNTAX_BY_UID, UNSUPPORTED_SYNTAXES, codec_cases,
                    codec_valid, iod_validator_lines, requirement_cases, requirement_matches, wire_cases, wire_valid)
from support import declared_support

STORAGE_PREFIX = '1.2.840.10008.5.1.4.1.1.'


class CorpusEnumeration(unittest.TestCase):
    def setUp(self):
        self.declared = declared_support(ROOT)
        self.sop_by_uid = {sop.uid: sop for sop in STORAGE_SOPS}

    def test_requirement_patterns_keep_uids_and_keys_whole(self):
        ids = ['corpus.negative.scp.1.2.7', 'corpus.negative.scp.1.2.7.1',
               'corpus.wire.scp.1.2.7.explicit-le', 'corpus.wire.scp.1.2.7.1.explicit-le',
               'corpus.codec.encode.jpeg-ls.mono8.f1', 'corpus.codec.encode.jpeg-ls-near.mono8.f1']
        self.assertEqual(requirement_matches('corpus.negative.scp.1.2.7', ids), ['corpus.negative.scp.1.2.7'])
        self.assertEqual(requirement_matches('corpus.wire.scp.1.2.7', ids), ['corpus.wire.scp.1.2.7.explicit-le'])
        self.assertEqual(requirement_matches('corpus.wire.scp.*.explicit-le', ids),
                         ['corpus.wire.scp.1.2.7.explicit-le', 'corpus.wire.scp.1.2.7.1.explicit-le'])
        self.assertEqual(requirement_matches('corpus.codec.*.jpeg-ls', ids), ['corpus.codec.encode.jpeg-ls.mono8.f1'])

    def test_catalog_is_exactly_the_declared_storage_contract(self):
        declared = {sop['uid'] for sop in self.declared['sop_classes'] if sop['uid'].startswith(STORAGE_PREFIX)}
        self.assertEqual(declared, set(self.sop_by_uid))
        self.assertEqual(len(STORAGE_SOPS), len(self.sop_by_uid))

    def test_syntaxes_are_exactly_the_declared_network_contract(self):
        accepted = {row['uid'] for row in self.declared['transfer_syntaxes'] if row['network'] == 'Accepted'}
        refused = {row['uid'] for row in self.declared['transfer_syntaxes'] if row['network'] != 'Accepted'}
        self.assertEqual(accepted, set(SYNTAX_BY_UID))
        self.assertEqual(refused, set(UNSUPPORTED_SYNTAXES))
        self.assertIn(BIG_ENDIAN, refused)
        self.assertIn(DEFLATED, refused)

    def test_every_pair_is_a_case_or_an_explained_exclusion(self):
        cases, exclusions = wire_cases()
        run = {(case['sop'], case['syntax'], case['role']) for case in cases}
        skipped = {(row['sop'], row['syntax']) for row in exclusions}
        self.assertTrue(all(row['reason'] for row in exclusions))
        for sop in STORAGE_SOPS:
            for syntax in NATIVE_SYNTAXES+COMPRESSED_SYNTAXES:
                if (sop.uid, syntax.uid) in skipped:
                    self.assertNotIn((sop.uid, syntax.uid, 'scp'), run)
                else:
                    for role in ('scp', 'scu', 'forward'):
                        self.assertIn((sop.uid, syntax.uid, role), run)

    def test_case_ids_are_unique(self):
        ids = [case['case_id'] for case in wire_cases()[0]+codec_cases()]
        self.assertEqual(len(ids), len(set(ids)))

    def test_objects_without_pixel_data_stay_native(self):
        for sop in STORAGE_SOPS:
            if sop.pixel_format is None:
                for syntax in COMPRESSED_SYNTAXES:
                    self.assertFalse(wire_valid(sop, syntax)[0], sop.name)
                for syntax in NATIVE_SYNTAXES:
                    self.assertTrue(wire_valid(sop, syntax)[0], sop.name)

    def test_iods_that_carry_pixels_are_not_treated_as_documents(self):
        # The issue's broad list called RT and SEG non-pixel; these classes carry images.
        for uid in ('1.2.840.10008.5.1.4.1.1.481.1', '1.2.840.10008.5.1.4.1.1.481.2', '1.2.840.10008.5.1.4.1.1.66.4',
                    '1.2.840.10008.5.1.4.1.1.66.7', '1.2.840.10008.5.1.4.1.1.66.8'):
            self.assertIsNotNone(self.sop_by_uid[uid].pixel_format)

    def test_format_rules_follow_the_standard(self):
        syntax = {item.key: item for item in COMPRESSED_SYNTAXES}
        self.assertFalse(codec_valid('mono12', syntax['jpeg-baseline'])[0])
        self.assertFalse(codec_valid('mono8s', syntax['jpeg-baseline'])[0])
        self.assertTrue(codec_valid('rgb8', syntax['jpeg-baseline'])[0])
        self.assertFalse(codec_valid('mono16s', syntax['jpeg-ls-near'])[0])
        self.assertTrue(codec_valid('mono16u', syntax['jpeg-ls-near'])[0])

    def test_forwarding_and_codec_support_are_decided_separately(self):
        # PACS decodes grayscale JPEG Lossless only, yet forwards color streams untouched.
        syntax = next(item for item in COMPRESSED_SYNTAXES if item.key == 'jpeg-lossless')
        color = next(sop for sop in STORAGE_SOPS if sop.pixel_format == 'rgb8')
        self.assertTrue(wire_valid(color, syntax)[0])
        self.assertFalse(codec_valid('rgb8', syntax)[0])

    def test_every_lossy_syntax_has_a_declared_bound_and_method(self):
        for syntax in COMPRESSED_SYNTAXES:
            if syntax.lossy:
                bound = LOSSY_BOUNDS[syntax.uid]
                self.assertTrue(bound.get('method'))
                self.assertTrue('max_abs_error' in bound or 'min_psnr_db' in bound)
            else:
                self.assertNotIn(syntax.uid, LOSSY_BOUNDS)

    def test_codec_matrix_covers_bit_depth_color_planar_and_frames(self):
        keys = {PIXEL_FORMATS[key] for key in CODEC_FORMATS}
        self.assertEqual({format_.bits_stored for format_ in keys}, {8, 12, 16})
        self.assertEqual({format_.signed for format_ in keys}, {True, False})
        self.assertEqual({format_.planar for format_ in keys}, {None, 0, 1})
        self.assertEqual({case['frames'] for case in codec_cases()}, {1, 3})

    def test_every_owned_requirement_names_its_evaluating_cases(self):
        owned = [case for case in self.declared['cases'] if case['owner_issue'] == 1318]
        mapping = requirement_cases(self.declared)
        self.assertEqual({case['case_id'] for case in owned}, set(mapping))
        self.assertTrue(all(pattern.startswith(('corpus.', 'file.')) for pattern in mapping.values()))

    def test_requirements_outside_this_scope_keep_their_owner(self):
        owners = {case['sop_class_uids'][0]: case['owner_issue'] for case in self.declared['cases'] if case['sop_class_uids']}
        self.assertEqual(owners['1.2.840.10008.5.1.4.1.2.2.1'], 1319)
        self.assertEqual(owners['1.2.840.10008.5.1.4.31'], 1320)

    def test_validator_conflicts_only_name_known_families(self):
        families = {sop.family for sop in STORAGE_SOPS}
        self.assertLessEqual(set(IOD_VALIDATOR_CONFLICTS), families)
        for rows in IOD_VALIDATOR_CONFLICTS.values():
            self.assertTrue(all(text and reason for text, reason in rows))

    def test_a_validator_that_died_gives_no_verdict(self):
        report = "dciodvfy: attrothr.cc:46: Assertion `bitsallocated <= bytesinword*8u' failed."
        with self.assertRaisesRegex(ValueError, 'signal 6'):
            iod_validator_lines(-6, report)
        self.assertEqual(iod_validator_lines(0, ''), [])
        self.assertEqual(iod_validator_lines(1, 'RTDose\nError - missing\n'), ['RTDose', 'Error - missing'])


if __name__ == '__main__':
    unittest.main()
