import importlib.util
import io
import json
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("verify_release_version", ROOT / "scripts" / "verify_release_version.py")
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)

OVERLAY = Path("vcpkg-ports") / "kcenon-pacs-system" / "vcpkg.json"


class RealLayoutTests(unittest.TestCase):
    """The repository itself must verify against its CMake project() VERSION."""

    def test_repository_sources_agree(self):
        expected = verifier.cmake_version(ROOT / "CMakeLists.txt")
        out = io.StringIO()
        self.assertEqual(verifier.verify(expected, ROOT, out=out), 0, out.getvalue())
        self.assertIn(f"All version sources match {expected}", out.getvalue())

    def test_repository_uses_multiline_project_and_version_semver(self):
        text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertRegex(text, r"project\(pacs_system\s*\n\s+VERSION")
        self.assertIn("version-semver", json.loads((ROOT / "vcpkg.json").read_text(encoding="utf-8")))


class FixtureTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        for name in ("CMakeLists.txt", "vcpkg.json", "Doxyfile"):
            shutil.copy(ROOT / name, self.root / name)
        (self.root / OVERLAY).parent.mkdir(parents=True)
        shutil.copy(ROOT / OVERLAY, self.root / OVERLAY)
        self.version = verifier.cmake_version(self.root / "CMakeLists.txt")

    def run_verify(self, expected=None):
        out = io.StringIO()
        code = verifier.verify(expected or self.version, self.root, out=out)
        return code, out.getvalue()

    def set_json_version(self, relative, value, key="version-semver"):
        path = self.root / relative
        data = json.loads(path.read_text(encoding="utf-8"))
        for existing in verifier.VCPKG_VERSION_KEYS:
            data.pop(existing, None)
        data[key] = value
        path.write_text(json.dumps(data), encoding="utf-8")

    def test_copied_layout_passes(self):
        code, out = self.run_verify()
        self.assertEqual(code, 0, out)

    def test_expected_version_mismatch_fails(self):
        code, out = self.run_verify("1.0.0")
        self.assertEqual(code, 1)
        self.assertEqual(out.count("MISMATCH"), 4)

    def test_cmake_mismatch_is_reported(self):
        cmake = self.root / "CMakeLists.txt"
        cmake.write_text(cmake.read_text(encoding="utf-8").replace(f"VERSION {self.version}", "VERSION 9.9.9", 1))
        code, out = self.run_verify()
        self.assertEqual(code, 1)
        self.assertIn("MISMATCH CMakeLists.txt project() VERSION: 9.9.9", out)

    def test_root_vcpkg_mismatch_is_reported(self):
        self.set_json_version("vcpkg.json", "9.9.9")
        code, out = self.run_verify()
        self.assertEqual(code, 1)
        self.assertIn("MISMATCH vcpkg.json: 9.9.9", out)

    def test_overlay_mismatch_is_reported(self):
        self.set_json_version(OVERLAY, "9.9.9")
        code, out = self.run_verify()
        self.assertEqual(code, 1)
        self.assertIn("MISMATCH vcpkg-ports/kcenon-pacs-system/vcpkg.json: 9.9.9", out)

    def test_doxygen_mismatch_is_reported(self):
        doxyfile = self.root / "Doxyfile"
        text = doxyfile.read_text(encoding="utf-8")
        doxyfile.write_text(text.replace(f'"{self.version}"', '"9.9.9"', 1), encoding="utf-8")
        code, out = self.run_verify()
        self.assertEqual(code, 1)
        self.assertIn("MISMATCH Doxyfile PROJECT_NUMBER: 9.9.9", out)

    def test_plain_version_key_is_accepted(self):
        self.set_json_version("vcpkg.json", self.version, key="version")
        self.assertEqual(self.run_verify()[0], 0)

    def test_multiple_version_keys_are_malformed(self):
        path = self.root / "vcpkg.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        data["version"] = self.version
        path.write_text(json.dumps(data), encoding="utf-8")
        code, out = self.run_verify()
        self.assertEqual(code, 2)
        self.assertIn("expected exactly one of", out)

    def test_invalid_json_is_malformed(self):
        (self.root / "vcpkg.json").write_text('{"version-semver": "0.1.0",', encoding="utf-8")
        code, out = self.run_verify()
        self.assertEqual(code, 2)
        self.assertIn("invalid JSON", out)

    def test_missing_overlay_is_reported(self):
        (self.root / OVERLAY).unlink()
        code, out = self.run_verify()
        self.assertEqual(code, 2)
        self.assertIn("not found", out)

    def test_missing_project_number_is_reported(self):
        doxyfile = self.root / "Doxyfile"
        lines = [line for line in doxyfile.read_text(encoding="utf-8").splitlines()
                 if not line.lstrip().startswith("PROJECT_NUMBER")]
        doxyfile.write_text("\n".join(lines), encoding="utf-8")
        code, out = self.run_verify()
        self.assertEqual(code, 2)
        self.assertIn("PROJECT_NUMBER", out)

    def test_invalid_expected_version_is_rejected(self):
        self.assertEqual(self.run_verify("v0.1")[0], 2)


class CMakeParsingTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.path = Path(tmp.name) / "CMakeLists.txt"

    def parse(self, text):
        self.path.write_text(text, encoding="utf-8")
        return verifier.cmake_version(self.path)

    def test_single_line_project(self):
        self.assertEqual(self.parse("project(pacs_system VERSION 2.3.4 LANGUAGES CXX)\n"), "2.3.4")

    def test_multiline_project_with_comments_and_parenthesized_description(self):
        text = ('# project(pacs_system VERSION 0.0.1)\n'
                'cmake_minimum_required(VERSION 3.20)\n'
                'project(pacs_system  # the product\n'
                '    DESCRIPTION "PACS (Picture Archiving) VERSION 7.7.7"\n'
                '    VERSION 1.2.3\n'
                '    LANGUAGES CXX\n'
                ')\n')
        self.assertEqual(self.parse(text), "1.2.3")

    def test_other_project_names_are_ignored(self):
        text = "project(helper VERSION 5.0.0)\nproject(pacs_system VERSION 0.4.0)\n"
        self.assertEqual(self.parse(text), "0.4.0")

    def test_missing_version_argument_fails(self):
        with self.assertRaises(verifier.SourceError):
            self.parse("project(pacs_system LANGUAGES CXX)\n")

    def test_missing_project_fails(self):
        with self.assertRaises(verifier.SourceError):
            self.parse("cmake_minimum_required(VERSION 3.20)\n")


if __name__ == "__main__":
    unittest.main()
