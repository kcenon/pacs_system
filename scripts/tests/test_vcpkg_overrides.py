import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("overrides", Path(__file__).parents[1] / "check_vcpkg_overrides.py")
overrides = importlib.util.module_from_spec(spec)
spec.loader.exec_module(overrides)


class VcpkgOverridesTests(unittest.TestCase):
    """Use a real local Git registry: no network, vcpkg binary, or mock git."""

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.registry = self.root / "registry"
        self.registry.mkdir()
        self.git("init", "-q")
        self.git("config", "user.name", "Offline fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        self.db = self.registry / "versions/o-/openjph.json"
        self.db.parent.mkdir(parents=True)
        self.rows = [{"version": "0.27.2", "port-version": 1, "git-tree": "a" * 40},
                     {"version": "0.27.0", "port-version": 0, "git-tree": "b" * 40}]
        self.write_database(self.rows)
        self.baseline = self.commit()
        self.manifest = {"overrides": [{"name": "openjph", "version": "0.27.2", "port-version": 1}]}
        self.config = {"default-registry": {"kind": "builtin", "baseline": self.baseline}}
        self.write_inputs()

    def git(self, *args):
        return subprocess.run(["git", "-C", str(self.registry), *args], check=True,
                              text=True, capture_output=True).stdout.strip()

    def commit(self):
        self.git("add", ".")
        self.git("commit", "-qm", "fixture")
        return self.git("rev-parse", "HEAD")

    def write_database(self, rows):
        self.db.write_text(json.dumps({"versions": rows}))

    def write_inputs(self):
        (self.root / "vcpkg.json").write_text(json.dumps(self.manifest))
        (self.root / "vcpkg-configuration.json").write_text(json.dumps(self.config))

    def check(self):
        return overrides.check(self.root, self.registry)

    def test_valid_set_checks_all_version_schemes_and_default_port_zero(self):
        for index, field in enumerate(overrides.VERSION_FIELDS):
            name = f"port-{index}"
            path = self.registry / f"versions/p-/{name}.json"
            path.parent.mkdir(exist_ok=True)
            path.write_text(json.dumps({"versions": [{field: "2026-01-01", "git-tree": "c" * 40}]}))
            self.manifest["overrides"].append({"name": name, field: "2026-01-01"})
        self.config["default-registry"]["baseline"] = self.commit()
        self.write_inputs()
        result = self.check()
        self.assertEqual(len(result["available"]), 5)
        self.assertEqual(result["missing"], [])

    def test_unavailable_version(self):
        self.manifest["overrides"][0]["version"] = "0.21.5"
        self.write_inputs()
        self.assertEqual(self.check()["missing"][0]["version"], "0.21.5")

    def test_wrong_port_version_and_omitted_zero_are_not_wildcards(self):
        for value in [2, None]:
            if value is None:
                self.manifest["overrides"][0].pop("port-version")
            else:
                self.manifest["overrides"][0]["port-version"] = value
            self.write_inputs()
            self.assertEqual(len(self.check()["missing"]), 1)

    def test_absent_port(self):
        self.manifest["overrides"][0]["name"] = "absent"
        self.write_inputs()
        self.assertEqual(self.check()["missing"][0]["reason"], "version database absent")

    def test_exact_baseline_ignores_newer_head_and_dirty_worktree(self):
        self.write_database(self.rows + [{"version": "0.21.5", "git-tree": "c" * 40}])
        self.commit()
        self.manifest["overrides"][0] = {"name": "openjph", "version": "0.21.5"}
        self.write_inputs()
        self.assertEqual(len(self.check()["missing"]), 1)
        self.db.write_text("invalid dirty worktree")
        self.assertEqual(len(self.check()["missing"]), 1)
        self.assertEqual(self.check()["baseline"], self.baseline)

    def test_manifest_baseline_and_embedded_configuration(self):
        (self.root / "vcpkg-configuration.json").unlink()
        for entry in [{"builtin-baseline": self.baseline}, {"vcpkg-configuration": self.config}]:
            (self.root / "vcpkg.json").write_text(json.dumps({"overrides": self.manifest["overrides"], **entry}))
            self.assertEqual(len(self.check()["available"]), 1)

    def test_git_replacement_refs_cannot_change_the_baseline_database(self):
        self.write_database(self.rows + [{"version": "0.21.5", "git-tree": "c" * 40}])
        replacement = self.commit()
        self.git("replace", self.baseline, replacement)
        self.manifest["overrides"][0] = {"name": "openjph", "version": "0.21.5"}
        self.write_inputs()
        self.assertEqual(len(self.check()["missing"]), 1)

    def test_invalid_baseline_and_registry_selection_fail_closed(self):
        for config, manifest in [
            ({}, self.manifest),
            ({"default-registry": None}, self.manifest),
            ({"default-registry": {"kind": "git", "baseline": self.baseline}}, self.manifest),
            ({"default-registry": {"baseline": "HEAD"}}, self.manifest),
            ({"default-registry": {"baseline": "f" * 40}}, self.manifest),
            (self.config, {**self.manifest, "builtin-baseline": "e" * 40}),
            (self.config, {**self.manifest, "vcpkg-configuration": self.config}),
            ({**self.config, "registries": [{"packages": ["open*"]}]}, self.manifest),
        ]:
            with self.subTest(config=config, manifest=manifest):
                (self.root / "vcpkg.json").write_text(json.dumps(manifest))
                (self.root / "vcpkg-configuration.json").write_text(json.dumps(config))
                with self.assertRaises(ValueError):
                    self.check()

    def test_invalid_manifest_and_override_shapes(self):
        for manifest in [[], {}, {"overrides": []}, {"overrides": {}},
                         {"overrides": [None]}, {"overrides": [{"name": "../bad", "version": "1"}]},
                         {"overrides": self.manifest["overrides"] * 2}]:
            (self.root / "vcpkg.json").write_text(json.dumps(manifest))
            with self.assertRaises(ValueError):
                self.check()
        for field, value in [("port-version", True), ("port-version", -1), ("port-version", "1"),
                             ("version", ""), ("version", 1), ("version-semver", "0.27.2")]:
            self.write_inputs()
            entry = {**self.manifest["overrides"][0], field: value}
            (self.root / "vcpkg.json").write_text(json.dumps({"overrides": [entry]}))
            with self.assertRaises(ValueError):
                self.check()

    def test_bad_json_and_duplicate_keys(self):
        for text in ["{bad", '{"overrides": [], "overrides": []}']:
            (self.root / "vcpkg.json").write_text(text)
            with self.assertRaises(ValueError):
                self.check()

    def test_invalid_database_is_an_input_error(self):
        for data in ["[]", '{"versions": {}}', "{bad",
                     json.dumps({"versions": [{"version": "1", "git-tree": "bad"}]})]:
            self.db.write_text(data)
            self.config["default-registry"]["baseline"] = self.commit()
            self.write_inputs()
            with self.assertRaises(ValueError):
                self.check()

    def test_historical_duplicate_version_rows_preserve_all_trees(self):
        self.write_database(self.rows + [{**self.rows[0], "git-tree": "d" * 40},
                                        {"version-string": "", "git-tree": "e" * 40}])
        self.config["default-registry"]["baseline"] = self.commit()
        self.write_inputs()
        result = self.check()
        self.assertEqual(result["missing"], [])
        self.assertEqual(result["available"][0]["git-trees"], ["a" * 40, "d" * 40])

    def test_cli_exits_json_and_ci_rejection(self):
        report = self.root / "report.json"
        args = ["--project-root", str(self.root), "--vcpkg-root", str(self.registry),
                "--json-output", str(report)]
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(overrides.main(args), 0)
            self.assertEqual(json.loads(report.read_text())["baseline"], self.baseline)
            self.manifest["overrides"][0]["version"] = "never-registered"
            self.write_inputs()
            self.assertEqual(overrides.main(args), 1)
            self.assertEqual(len(json.loads(report.read_text())["missing"]), 1)
            (self.root / "vcpkg.json").unlink()
            self.assertEqual(overrides.main(args), 2)


if __name__ == "__main__":
    unittest.main()
