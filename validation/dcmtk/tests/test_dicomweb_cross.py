"""Standard-library checks of the DICOMweb tables; running them against servers is dicomweb_probe.py's job."""
import ast
import fnmatch
from pathlib import Path
import re
import struct
import sys
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'validation/dcmtk'))
sys.path.insert(0, str(ROOT/'validation/peers/http'))
from dicomweb_cross import (ACCEPT_CASES, FRAME_REFUSALS, PAGE_SIZES, QIDO_BAD_PARAMETERS, QIDO_FILTERS, QIDO_STUDIES, READ, REFERENCE_DIFFERENCES,
                            REFUSED_TOKENS, ROUTES, SEARCH, STOW_ALL_FAILED, STOW_PARTIAL, STOW_REFUSALS, UNAUTHENTICATED_ROUTES, WRITE, requirement_cases)
from http_peer import PayloadError, build_multipart, read_jpeg, read_png, split_multipart

PROBE = ROOT/'validation/dcmtk/tests/dicomweb_probe.py'
PEER = ROOT/'validation/peers/http/http_peer.py'
SOURCES = ('src/web/endpoints/dicomweb_endpoints.cpp', 'src/web/endpoints/storage_commitment_endpoints.cpp', 'src/web/endpoints/wado_uri_endpoints.cpp')


class RouteTable(unittest.TestCase):
    def registered(self):
        """Every (method, path) the source registers, with Crow's placeholders."""
        routes = set()
        for name in SOURCES:
            for path, method in re.findall(r'CROW_ROUTE\(\s*app,\s*"([^"]+)"\s*\)\s*\.methods\(crow::HTTPMethod::(\w+)\)', (ROOT/name).read_text()):
                routes.add((method, path))
        return routes

    @staticmethod
    def as_crow(template):
        if template.startswith('@'):
            return template[1:].split('?')[0]
        return '/dicomweb'+re.sub(r'\{\w+\}', '<string>', template)

    def test_every_registered_route_has_an_authorization_row(self):
        listed = {(method, self.as_crow(template)) for _, method, template, _ in ROUTES} | set(UNAUTHENTICATED_ROUTES)
        self.assertEqual(self.registered(), listed)

    def test_each_route_names_scopes_that_exist(self):
        for name, method, _, scopes in ROUTES:
            self.assertTrue(scopes, name)
            self.assertLessEqual(set(scopes), {READ, SEARCH, WRITE}, name)
            if method == 'POST':
                self.assertEqual(scopes, (WRITE,), name)

    def test_nothing_read_is_opened_by_the_write_scope(self):
        for name, method, _, scopes in ROUTES:
            if method == 'GET':
                self.assertNotIn(WRITE, scopes, name)

    def test_every_kind_of_bad_token_is_listed(self):
        for kind in ('no-token', 'wrong-signature', 'expired', 'not-yet-valid', 'wrong-issuer', 'wrong-audience', 'alg-none', 'hs256-with-public-key'):
            self.assertIn(kind, REFUSED_TOKENS)


class ContractTables(unittest.TestCase):
    def test_standard_statuses_only(self):
        for name, _, _, status, shape in ACCEPT_CASES:
            self.assertIn(status, (200, 400, 406), name)
            self.assertEqual(status == 200, shape is not None, name)

    def test_every_resource_refuses_a_missing_or_unusable_accept(self):
        """8.7.5 and 10.4.3.1 are for every resource, so a resource without these rows is not covered."""
        resources = {row[1] for row in ACCEPT_CASES}
        refused = {row[1] for row in ACCEPT_CASES if row[3] == 406}
        self.assertEqual(resources, refused)

    def test_a_single_part_type_is_refused_on_a_collection(self):
        """10.4.4: application/dicom "shall only be used for individual Instance Resources"."""
        rows = {row[0]: row for row in ACCEPT_CASES}
        for name in ('study.single-part-only', 'series.single-part-only'):
            self.assertEqual(rows[name][2:4], ('application/dicom', 406))
        self.assertEqual(rows['instance.single-part'][3], 200)

    def test_frame_refusals_cover_silent_dropping(self):
        rows = {name: (frames, statuses) for name, frames, statuses in FRAME_REFUSALS}
        for name in ('one-good-one-past', 'one-past-one-good', 'one-bad-syntax'):
            self.assertIn(name, rows)
            self.assertNotIn(200, rows[name][1])

    def test_a_refused_store_has_a_status_of_table_10_5_3_1(self):
        for _, status in STOW_REFUSALS:
            self.assertIn(status, (400, 415))
        self.assertLessEqual(set(STOW_PARTIAL), set(STOW_ALL_FAILED))

    def test_search_tables_name_known_studies(self):
        keys = {row['key'] for row in QIDO_STUDIES}
        for name, _, expected in QIDO_FILTERS:
            self.assertLessEqual(set(expected), keys, name)

    def test_wildcard_look_alikes_have_a_decoy(self):
        """A LIKE pattern would match the decoy: that is what the row is for."""
        ids = {row['key']: row['PatientID'] for row in QIDO_STUDIES}
        self.assertEqual(ids['delta'], 'QID_4')
        self.assertEqual(ids['delta-x'], 'QIDX4')
        rows = {name: expected for name, _, expected in QIDO_FILTERS}
        self.assertEqual(rows['literal-underscore-in-id'], ('delta',))
        self.assertEqual(rows['literal-percent-in-name'], ('delta',))

    def test_paging_crosses_the_default_limit(self):
        self.assertTrue(PAGE_SIZES)
        self.assertGreater(max(PAGE_SIZES), 1)

    def test_bad_parameters_are_unique(self):
        names = [name for name, _ in QIDO_BAD_PARAMETERS]
        self.assertEqual(len(names), len(set(names)))


class Requirements(unittest.TestCase):
    def source(self):
        return PROBE.read_text()

    def test_the_two_requirements_are_mapped(self):
        self.assertEqual(set(requirement_cases()), {'requirement.web.positive', 'requirement.web.negative'})

    def test_refusals_are_negative_cases(self):
        patterns = requirement_cases()['requirement.web.negative']
        for case in ('web.pacs.auth.refused.study.no-token', 'web.pacs.auth.scope.series', 'web.pacs.negative.stow.truncated', 'web.pacs.limits.body'):
            self.assertTrue(any(fnmatch.fnmatchcase(case, pattern) for pattern in patterns), case)

    def test_stored_objects_are_positive_cases(self):
        patterns = requirement_cases()['requirement.web.positive']
        for case in ('web.pacs.stow.get.each', 'web.pacs.cstore.frames.native', 'web.pacs.qido.page.studies.7', 'web.pacs.concurrent.both-protocols', 'web.pacs.restart.objects-survive'):
            self.assertTrue(any(fnmatch.fnmatchcase(case, pattern) for pattern in patterns), case)


class Independence(unittest.TestCase):
    def imports_of(self, path):
        tree = ast.parse(path.read_text())
        names = set()
        for node in ast.walk(tree):
            if isinstance(node, ast.Import):
                names |= {alias.name.split('.')[0] for alias in node.names}
            elif isinstance(node, ast.ImportFrom) and node.module:
                names.add(node.module.split('.')[0])
        return names

    def test_the_peer_helper_shares_no_product_code(self):
        self.assertEqual(self.imports_of(PEER) & {'kcenon', 'pacs', 'pacs_system'}, set())
        self.assertNotIn('kcenon', PEER.read_text())

    def test_a_reference_difference_never_excuses_pacs(self):
        source = PROBE.read_text()
        self.assertIn("if not target.is_pacs else None", source)
        self.assertIn("self.lenient_for = () if target.is_pacs", source)

    def test_every_difference_names_a_case_that_runs(self):
        source = PROBE.read_text()
        for pattern in REFERENCE_DIFFERENCES:
            first = pattern.lstrip('*.').split('.')[0]
            self.assertIn(first, source, pattern)


class Statement(unittest.TestCase):
    text = (ROOT/'docs/DICOM_CONFORMANCE_STATEMENT.md').read_text()

    def section(self, heading):
        start = self.text.index(heading)
        return self.text[start:self.text.index('\n### ', start+len(heading))]

    def test_it_declares_what_10_6_5_asks_for(self):
        section = self.section('### 8.3 QIDO-RS')
        for words in ('Maximum matches', 'Fuzzy Matching', 'Empty Value Matching', 'Multiple Value Matching', 'includefield'):
            self.assertIn(words, section)

    def test_the_declared_limits_are_the_ones_in_the_code(self):
        source = (ROOT/'src/web/endpoints/dicomweb_endpoints.cpp').read_text()
        default = re.search(r'default_search_limit = (\d+);', source)[1]
        largest = re.search(r'largest_search_page = (\d+);', source)[1]
        section = self.section('### 8.3 QIDO-RS')
        self.assertIn(f'{default} per response unless `limit` asks otherwise, and at most {largest} in one response', section)

    def test_it_does_not_claim_a_fallback_the_code_lacks(self):
        section = self.section('### 8.5 OAuth 2.0')
        self.assertNotIn('X-User-ID', section)
        self.assertIn('no authorization at all', section)

    def test_every_route_is_guarded_in_the_source(self):
        """A DICOMweb route either goes through guarded() or one of the helpers built on it."""
        source = (ROOT/'src/web/endpoints/dicomweb_endpoints.cpp').read_text()
        routes = source[source.index('void register_dicomweb_endpoints_impl'):]
        handlers = re.split(r'CROW_ROUTE\(app, ', routes)[1:]
        for handler in handlers:
            if 'HTTPMethod::OPTIONS' in handler:
                continue
            self.assertRegex(handler, r'guarded\(|bulk_response\(|frames_response\(|rendered_response\(|store_instances\(|search_response\(', handler[:80])
        for helper in ('bulk_response', 'frames_response', 'rendered_response', 'search_response', 'store_instances'):
            body = source[source.index(f'crow::response {helper}('):]
            self.assertIn('return guarded(ctx, req,', body[:600], helper)


class Readers(unittest.TestCase):
    """The strict readers judge the server, so they are held to their formats here."""

    def test_multipart_round_trip_keeps_binary(self):
        data = bytes(range(256))+b'\r\n--almost-a-boundary\r\n'
        kind, body = build_multipart([('application/dicom', data), ('application/dicom', b'')])
        parts = split_multipart(kind, body)
        self.assertEqual([part.data for part in parts], [data, b''])

    def test_multipart_refuses_what_a_lenient_reader_would_shorten(self):
        kind, body = build_multipart([('application/dicom', b'x')])
        with self.assertRaises(PayloadError):
            split_multipart(kind, body[:-10])
        with self.assertRaises(PayloadError):
            split_multipart('multipart/related; boundary=probe-boundary-7f3a9c', body)
        with self.assertRaises(PayloadError):
            split_multipart('application/dicom', body)

    def test_png_reader_with_a_known_file(self):
        # Two rows of 8 bit gray, each with a filter that needs undoing, written from the format's definition.
        def chunk(kind, data):
            return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data))
        raw = bytes((1, 10, 20))+bytes((2, 1, 2))  # Sub: 10, 30; Up: 11, 32
        data = b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 0, 0, 0, 0))+chunk(b'IDAT', zlib.compress(raw))+chunk(b'IEND', b'')
        image = read_png(data)
        self.assertEqual((image['width'], image['height'], image['channels'], image['samples']), (2, 2, 1, bytes((10, 30, 11, 32))))
        with self.assertRaises(PayloadError):
            read_png(b'not a png')
        with self.assertRaises(PayloadError):
            read_png(data[:-6]+b'xxxxxx')

    def test_jpeg_reader_finds_the_frame_header(self):
        data = bytes.fromhex('ffd8ffc0000b080006000801011100ffd9')
        self.assertEqual({key: read_jpeg(data)[key] for key in ('width', 'height', 'components')}, dict(width=8, height=6, components=1))
        with self.assertRaises(PayloadError):
            read_jpeg(b'\xff\xd8no end')


if __name__ == '__main__':
    unittest.main()
