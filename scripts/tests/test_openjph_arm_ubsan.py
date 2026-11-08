import contextlib
import importlib.util
import io
import json
from pathlib import Path
import shlex
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location(
    "arm_ubsan", Path(__file__).parents[1] / "check_openjph_arm_ubsan.py")
arm_ubsan = importlib.util.module_from_spec(spec)
spec.loader.exec_module(arm_ubsan)

# Stands in for the real test binaries: this exercises the checker's decisions,
# and is no evidence about OpenJPH. Real runs are recorded by the ARM CI job.
FAKE_TOOL = """#!/usr/bin/env python3
import json, os, pathlib, sys
tool = pathlib.Path(sys.argv[0])
rules = json.loads((tool.parent / "behaviour.json").read_text())
args = sys.argv[1:]
out = args[args.index("--out") + 1] if "--out" in args else None
selection = args[:args.index("--reporter")] if "--reporter" in args else args
mode = "tracked" if "suppressions=" in os.environ.get("UBSAN_OPTIONS", "") else "bare"
rule = rules.get("|".join([tool.name, " ".join(selection), mode]))
if rule is None:
    sys.exit("fake tool has no rule for this exact command and suppression mode")
sys.stderr.write(rule.get("log", ""))
if out and "cases" in rule:
    cases = "".join('<TestCase name="%s"/>' % name for name in rule["cases"])
    pathlib.Path(out).write_text('<Catch2TestRun>%s<OverallResultsCases successes="%d" '
                                 'failures="0" expectedFailures="0" skips="0"/></Catch2TestRun>'
                                 % (cases, len(rule["cases"])))
sys.exit(rule.get("exit", 0))
"""

COMPILE_FLAGS = "/usr/bin/c++ -fsanitize=undefined -O1 -std=c++{standard} -c source.cpp"
OBJECTS = {
    "openjph": "_deps/openjph-build/src/core/CMakeFiles/openjph.dir/transform/ojph_colour.cpp.o",
    "control14": "CMakeFiles/pacs_ubsan_shift_control.dir/tests/sanitizers/shift_base_control.cpp.o",
    "control20": "CMakeFiles/pacs_ubsan_shift_control_cxx20.dir/tests/sanitizers/shift_base_control.cpp.o",
}
ALL_CASES = sorted(arm_ubsan.REQUIRED_CASES) + [f"extra case {n}" for n in range(13)]


def report_at(source, line, message):
    return f"/src/{source}:{line}:29: runtime error: {message} -128\n"


class ArmUbsanCheckerTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.build = self.root / "build"
        self.bin = self.build / "bin"
        self.bin.mkdir(parents=True)
        self.output = self.root / "results"
        self.suppression = self.root / "ubsan.supp"
        self.suppression.write_text("# tracked\n" + arm_ubsan.SUPPRESSION_ENTRY + "\n")
        for name in ("encoding_tests", "pacs_ubsan_shift_control", "pacs_ubsan_shift_control_cxx20"):
            tool = self.bin / name
            tool.write_text(FAKE_TOOL)
            tool.chmod(0o755)
        for relative in OBJECTS.values():
            target = self.build / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"\x7fELF U __ubsan_handle_add_overflow")
        self.standards = {"openjph": 14, "control14": 14, "control20": 20}
        self.rules = {
            "pacs_ubsan_shift_control||tracked": {
                "exit": 1, "log": report_at(arm_ubsan.CONTROL_SOURCE, 26, arm_ubsan.NEGATIVE_SHIFT)},
            "pacs_ubsan_shift_control_cxx20||bare": {"exit": 0},
            "pacs_ubsan_shift_control_cxx20|overflow|bare": {
                "exit": 1, "log": report_at(arm_ubsan.CONTROL_SOURCE, 22, arm_ubsan.SIGNED_OVERFLOW)},
        }

    def expect_cxx14(self):
        for name, case in arm_ubsan.PROBES.items():
            line = 485 if name == "generic64" else 467
            self.rules[f"encoding_tests|{case}|bare"] = {
                "exit": 1, "log": report_at("ojph_colour.cpp", line, arm_ubsan.NEGATIVE_SHIFT)}
        self.rules["encoding_tests|[htj2k]|tracked"] = {"exit": 0, "cases": ALL_CASES}

    def expect_cxx20(self):
        self.standards["openjph"] = 20
        self.rules["encoding_tests|[htj2k]|bare"] = {"exit": 0, "cases": ALL_CASES}

    def write_compile_commands(self):
        entries = [{"directory": str(self.build), "file": "source.cpp",
                    "command": COMPILE_FLAGS.format(standard=self.standards[key]), "output": relative}
                   for key, relative in OBJECTS.items()]
        (self.build / "compile_commands.json").write_text(json.dumps(entries))

    def check(self, standard, machine="aarch64"):
        (self.bin / "behaviour.json").write_text(json.dumps(self.rules))
        if not (self.build / "compile_commands.json").exists():
            self.write_compile_commands()
        stdout, stderr = io.StringIO(), io.StringIO()
        with mock.patch("platform.machine", return_value=machine), \
                contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = arm_ubsan.main(["--build-dir", str(self.build), "--output", str(self.output),
                                   "--openjph-standard", str(standard), "--suppression", str(self.suppression)])
        return code, json.loads((self.output / "results.json").read_text())

    def assert_rejected(self, standard, reason):
        code, report = self.check(standard)
        self.assertEqual(code, 1, report)
        self.assertEqual(report["result"], "failed")
        self.assertIn(reason, report["error"])
        return report

    def test_cxx14_reproduces_each_probe_then_tracked_suite_passes(self):
        self.expect_cxx14()
        code, report = self.check(14)
        self.assertEqual(code, 0, report)
        self.assertEqual(report["reproduced_at_line"],
                         {"rgb": 467, "output-parameters": 467, "generic32": 467, "generic64": 485})
        self.assertEqual(report["htj2k_cases"]["successes"], len(ALL_CASES))
        self.assertNotIn("suppressions=", report["commands"]["unsuppressed-rgb"]["UBSAN_OPTIONS"])
        self.assertIn("suppressions=", report["commands"]["tracked-htj2k"]["UBSAN_OPTIONS"])

    def test_cxx20_passes_with_no_suppression_file(self):
        self.expect_cxx20()
        code, report = self.check(20)
        self.assertEqual(code, 0, report)
        self.assertNotIn("suppressions=", report["commands"]["unsuppressed-htj2k"]["UBSAN_OPTIONS"])
        self.assertNotIn("tracked-htj2k", report["commands"])

    def test_cxx14_rejects_probe_that_no_longer_reproduces(self):
        # The original #1299 failure: a C++20 dependency passed the RGB probe.
        self.expect_cxx14()
        self.rules[f"encoding_tests|{arm_ubsan.PROBES['rgb']}|bare"] = {"exit": 0}
        self.assert_rejected(14, "unsuppressed-rgb")

    def test_cxx14_rejects_a_report_from_another_source(self):
        self.expect_cxx14()
        self.rules[f"encoding_tests|{arm_ubsan.PROBES['rgb']}|bare"] = {
            "exit": 1, "log": report_at("ojph_transform.cpp", 409, arm_ubsan.NEGATIVE_SHIFT)}
        self.assert_rejected(14, "unsuppressed-rgb")

    def test_cxx14_rejects_generic_probes_on_one_branch(self):
        self.expect_cxx14()
        self.rules[f"encoding_tests|{arm_ubsan.PROBES['generic64']}|bare"]["log"] = report_at(
            "ojph_colour.cpp", 467, arm_ubsan.NEGATIVE_SHIFT)
        self.assert_rejected(14, "separate branches")

    def test_cxx14_rejects_sanitizer_report_under_suppression(self):
        self.expect_cxx14()
        self.rules["encoding_tests|[htj2k]|tracked"]["log"] = report_at(
            "ojph_transform.cpp", 409, arm_ubsan.NEGATIVE_SHIFT)
        self.assert_rejected(14, "tracked-htj2k")

    def test_cxx20_rejects_any_sanitizer_report(self):
        self.expect_cxx20()
        self.rules["encoding_tests|[htj2k]|bare"]["log"] = report_at(
            "ojph_colour.cpp", 467, arm_ubsan.NEGATIVE_SHIFT)
        self.assert_rejected(20, "unsuppressed-htj2k")

    def test_cxx20_never_loads_the_suppression_for_the_suite(self):
        self.expect_cxx20()
        del self.rules["encoding_tests|[htj2k]|bare"]
        self.rules["encoding_tests|[htj2k]|tracked"] = {"exit": 0, "cases": ALL_CASES}
        self.assert_rejected(20, "unsuppressed-htj2k")

    def test_actual_standard_must_match_the_mode_under_test(self):
        self.expect_cxx14()
        self.standards["openjph"] = 20
        report = self.assert_rejected(14, "expected C++14")
        self.assertEqual(report["commands"], {})
        (self.build / "compile_commands.json").unlink()
        self.expect_cxx20()
        self.standards["openjph"] = 14
        report = self.assert_rejected(20, "expected C++20")
        self.assertEqual(report["commands"], {})

    def test_control_standards_are_verified(self):
        self.expect_cxx20()
        self.standards["control20"] = 14
        self.assert_rejected(20, "pacs_ubsan_shift_control_cxx20.dir/: expected C++20")

    def test_uninstrumented_dependency_is_rejected(self):
        self.expect_cxx20()
        self.write_compile_commands()
        path = self.build / "compile_commands.json"
        entries = json.loads(path.read_text())
        entries[0]["command"] = entries[0]["command"].replace("-fsanitize=undefined ", "")
        path.write_text(json.dumps(entries))
        self.assert_rejected(20, "lacks -fsanitize=undefined")

    def test_object_without_sanitizer_handlers_is_rejected(self):
        self.expect_cxx20()
        (self.build / OBJECTS["openjph"]).write_bytes(b"\x7fELF no handlers")
        self.assert_rejected(20, "no UBSan handler references")

    def test_missing_suppression_is_rejected(self):
        self.expect_cxx14()
        self.suppression.write_text("# nothing tracked\n")
        self.assert_rejected(14, "suppression is missing")

    def test_wider_suppressions_are_rejected_in_both_modes(self):
        for standard, expect in ((14, self.expect_cxx14), (20, self.expect_cxx20)):
            for entry in ("shift-base:openjph-src", "alignment:ojph_mem.cpp", "shift-base:*"):
                with self.subTest(standard=standard, entry=entry):
                    expect()
                    self.suppression.write_text(f"{arm_ubsan.SUPPRESSION_ENTRY}\n{entry}\n")
                    self.assert_rejected(standard, "wider than the tracked")

    def test_unrelated_third_party_suppression_is_allowed(self):
        self.expect_cxx14()
        self.suppression.write_text(f"{arm_ubsan.SUPPRESSION_ENTRY}\nalignment:other-library/source.cpp\n")
        self.assertEqual(self.check(14)[0], 0)

    def test_incomplete_case_selection_is_rejected(self):
        self.expect_cxx20()
        self.rules["encoding_tests|[htj2k]|bare"]["cases"] = ALL_CASES[:-1]
        self.assert_rejected(20, "incomplete or failed")
        missing = [case for case in ALL_CASES if case != arm_ubsan.PROBES["generic64"]] + ["replacement"]
        self.rules["encoding_tests|[htj2k]|bare"]["cases"] = missing
        self.assert_rejected(20, "required cases not executed")

    def test_suppression_hiding_the_unrelated_control_is_rejected(self):
        self.expect_cxx14()
        self.rules["pacs_ubsan_shift_control||tracked"] = {"exit": 0}
        self.assert_rejected(14, "unrelated-shift-cxx14")

    def test_cxx20_control_reporting_the_shift_is_rejected(self):
        self.expect_cxx20()
        self.rules["pacs_ubsan_shift_control_cxx20||bare"] = {
            "exit": 1, "log": report_at(arm_ubsan.CONTROL_SOURCE, 26, arm_ubsan.NEGATIVE_SHIFT)}
        self.assert_rejected(20, "language-mode-cxx20")

    def test_silent_instrumentation_control_is_rejected(self):
        self.expect_cxx20()
        self.rules["pacs_ubsan_shift_control_cxx20|overflow|bare"] = {"exit": 0}
        self.assert_rejected(20, "instrumentation-cxx20")

    def test_non_arm_host_is_rejected(self):
        self.expect_cxx20()
        code, report = self.check(20, machine="x86_64")
        self.assertEqual(code, 1)
        self.assertIn("native ARM64", report["error"])
        self.assertEqual(report["commands"], {})

    def test_timed_out_probe_is_not_an_expected_failure(self):
        self.expect_cxx14()
        with mock.patch.object(arm_ubsan, "PROBE_TIMEOUT_SECONDS", 0.001):
            report = self.assert_rejected(14, "unsuppressed-rgb")
        self.assertEqual(report["commands"]["unsuppressed-rgb"]["exit"], arm_ubsan.TIMEOUT_EXIT)

    def test_compile_commands_are_recorded(self):
        self.expect_cxx20()
        _, report = self.check(20)
        unit = report["translation_units"]["openjph-colour"]
        self.assertEqual(unit["standard"], 20)
        self.assertEqual(unit["arguments"], shlex.split(COMPILE_FLAGS.format(standard=20)))
        self.assertEqual(Path(report["commands"]["unsuppressed-htj2k"]["argv"][0]).name, "encoding_tests")

    def test_stale_suite_report_does_not_count(self):
        self.expect_cxx20()
        self.assertEqual(self.check(20)[0], 0)
        del self.rules["encoding_tests|[htj2k]|bare"]["cases"]  # Exits 0 but writes no report.
        code, report = self.check(20)
        self.assertEqual(code, 1, report)


if __name__ == "__main__":
    unittest.main()
