"""Standard-library checks of the browser suite in web/tests/e2e (#1322): what it must never do and how it is pinned."""
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
E2E = ROOT/'web/tests/e2e'
SPECS = sorted(E2E.glob('*.spec.ts'))


class BrowserSuite(unittest.TestCase):
    def test_specs_exist_and_every_test_states_its_condition(self):
        self.assertTrue(SPECS)
        for spec in SPECS:
            text = spec.read_text()
            tests = re.findall(r"\btest\(", text)
            tagged = re.findall(r"tag: '@(positive|negative)'", text)
            self.assertTrue(tests, spec.name)
            # A loop of tests holds one tagged call for several cases, so count calls, not cases.
            self.assertEqual(len(tests), len(tagged), f'{spec.name}: every test names @positive or @negative')

    def test_nothing_stands_in_for_the_server(self):
        # Routing a request in the browser would let a page pass against a made-up answer.
        forbidden = re.compile(r'\.route\(|routeFromHAR|\.unroute\(|fulfill\(|mock', re.IGNORECASE)
        for path in [*SPECS, E2E/'support.ts', E2E/'playwright.config.ts']:
            self.assertIsNone(forbidden.search(path.read_text()), path.name)

    def test_playwright_is_pinned_exactly_and_the_lock_agrees(self):
        package = json.loads((ROOT/'web/package.json').read_text())
        lock = json.loads((ROOT/'web/package-lock.json').read_text())
        pinned = package['devDependencies']['@playwright/test']
        self.assertRegex(pinned, r'^\d+\.\d+\.\d+$')
        self.assertEqual(lock['packages']['node_modules/@playwright/test']['version'], pinned)
        self.assertEqual(lock['packages']['node_modules/playwright']['version'], pinned)
        self.assertEqual(lock['packages']['node_modules/playwright-core']['version'], pinned)

    def test_failures_keep_their_evidence(self):
        config = (E2E/'playwright.config.ts').read_text()
        self.assertIn("trace: 'retain-on-failure'", config)
        self.assertIn("screenshot: 'only-on-failure'", config)
        self.assertIn('forbidOnly: true', config)
        self.assertIn('retries: 0', config)


if __name__ == '__main__':
    unittest.main()
