#!/usr/bin/env python3
"""Validate OpenJPH generic RCT coverage on native ARM64 for one language mode.

OpenJPH 0.30.1 left-shifts negative green samples in its generic reversible
colour transform. Whether that is undefined depends on the C++ standard the
dependency is compiled with, so --openjph-standard names the mode under test and
compile_commands.json must prove the build really used it:

14  OpenJPH's own default and the vcpkg port's mode. The shift is undefined:
    unsuppressed probes must fail, and only the tracked source-specific
    suppression may let the HTJ2K suite pass.
20  What a FetchContent build inherits from pacs_system. The shift is defined,
    so the HTJ2K suite must pass with no suppression file at all. The pinned
    source is unchanged, so this is not an upstream fix.

Controls run in both modes, so neither result can come from a suppression wider
than OpenJPH's file or from a compiler that stopped instrumenting.

When a fixed release is adopted, remove the suppression and require the
unsuppressed pass in both modes (pacs_system#1299, OpenJPH#406/#405).
"""

import argparse
import functools
import json
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import sys
import xml.etree.ElementTree as ET

PROBES = {
    "rgb": "htj2k_codec lossless 8-bit RGB round-trip",
    "output-parameters": "htj2k_codec decode sets output parameters correctly",
    "generic32": "OpenJPH generic RCT 32-bit bounds and reversibility",
    "generic64": "OpenJPH generic RCT 64-bit bounds and reversibility",
}
REQUIRED_CASES = set(PROBES.values()) | {"OpenJPH ARM dispatch selects generic RCT"}
MINIMUM_HTJ2K_CASES = 18
SUPPRESSION_ENTRY = "shift-base:openjph-src/src/core/transform/ojph_colour.cpp"
BARE_OPTIONS = "halt_on_error=1:print_stacktrace=1"
NEGATIVE_SHIFT = "left shift of negative value"
SIGNED_OVERFLOW = "signed integer overflow"
CONTROL_SOURCE = "shift_base_control.cpp"
PROBE_TIMEOUT_SECONDS = 120
TIMEOUT_EXIT = 124

COVERAGE = {
    14: "tracked suppressed: OpenJPH compiled as C++14 executes the undefined shift",
    20: "unsuppressed: OpenJPH compiled as C++20 defines the shift; upstream source unchanged",
}


def verify_suppressions(path):
    """Require the tracked entry and reject anything else that could cover OpenJPH."""
    lines = (line.strip() for line in path.read_text(encoding="utf-8").splitlines())
    entries = [line for line in lines if line and not line.startswith("#")]
    if SUPPRESSION_ENTRY not in entries:
        raise ValueError("expected tracked, source-specific shift-base suppression is missing")
    wider = [entry for entry in entries if entry != SUPPRESSION_ENTRY and
             any(token in entry.lower() for token in ("ojph", "openjph", "htj2k", "*"))]
    if wider:
        raise ValueError(f"suppressions wider than the tracked RCT entry: {wider}")


def verify_translation_unit(entries, object_fragment, standard):
    """Require one instrumented compile of a source at the expected standard."""
    matches = [entry for entry in entries if object_fragment in entry.get("output", "")]
    if len(matches) != 1:
        raise ValueError(f"{object_fragment}: expected one compile command, found {len(matches)}")
    entry = matches[0]
    arguments = entry.get("arguments") or shlex.split(entry["command"])
    flags = [argument for argument in arguments if argument.startswith("-std=")]
    actual = re.fullmatch(r"-std=(?:c|gnu)\+\+(\d+)", flags[-1]) if flags else None
    if actual is None or int(actual.group(1)) != standard:
        raise ValueError(f"{object_fragment}: expected C++{standard}, "
                         f"compile command has {flags or 'no -std flag'}")
    if "-fsanitize=undefined" not in arguments:
        raise ValueError(f"{object_fragment}: compile command lacks -fsanitize=undefined")
    if b"__ubsan_handle_" not in (Path(entry["directory"]) / entry["output"]).read_bytes():
        raise ValueError(f"{object_fragment}: object has no UBSan handler references")
    return {"standard": standard, "arguments": arguments}


def verify_build(build_dir, standard):
    """Reject a build that cannot answer the question before running any probe."""
    entries = json.loads((build_dir / "compile_commands.json").read_text(encoding="utf-8"))
    return {
        "openjph-colour": verify_translation_unit(
            entries, "openjph.dir/transform/ojph_colour.cpp.o", standard),
        "control-cxx14": verify_translation_unit(entries, "pacs_ubsan_shift_control.dir/", 14),
        "control-cxx20": verify_translation_unit(entries, "pacs_ubsan_shift_control_cxx20.dir/", 20),
    }


def run_probe(output, report, name, argv, options):
    """Run one command, keeping its raw log, exit status and sanitizer options."""
    argv = [str(argument) for argument in argv]
    log = output / (name + ".log")
    environment = dict(os.environ, UBSAN_OPTIONS=options)
    with log.open("w", encoding="utf-8") as sink:
        try:
            code = subprocess.run(argv, env=environment, stdout=sink, stderr=subprocess.STDOUT,
                                  timeout=PROBE_TIMEOUT_SECONDS, check=False).returncode
        except subprocess.TimeoutExpired:
            sink.write("\nSanitizer probe timed out\n")
            code = TIMEOUT_EXIT
    report["commands"][name] = {"argv": argv, "exit": code, "UBSAN_OPTIONS": options, "log": log.name}
    return code, log.read_text(encoding="utf-8")


def reported_line(name, code, log, source, message):
    """Return the line of an expected report; any other outcome is a failure."""
    match = re.search(rf"{re.escape(source)}:(\d+):\d+: runtime error: {re.escape(message)}", log)
    if code in (0, TIMEOUT_EXIT) or match is None:
        raise ValueError(f"{name}: expected '{message}' from {source}; "
                         f"exit {code}, inspect {name}.log")
    return int(match.group(1))


def run_htj2k_suite(run, binary, output, name, options):
    """Require a complete, clean HTJ2K selection and return its case counts."""
    xml = output.resolve() / (name + ".xml")
    xml.unlink(missing_ok=True)  # A stale report from an earlier run must not count.
    code, log = run(name, [binary, "[htj2k]", "--reporter", "xml", "--out", xml], options)
    if code != 0 or "runtime error:" in log:
        raise ValueError(f"{name}: HTJ2K suite failed or emitted a sanitizer report; "
                         f"exit {code}, inspect {name}.log")
    root = ET.parse(xml).getroot()
    totals = root.find("OverallResultsCases")
    if totals is None:
        raise ValueError(f"{name}: Catch2 did not report case totals")
    counts = {key: int(totals.attrib.get(key, 0))
              for key in ("successes", "failures", "expectedFailures", "skips")}
    if counts["successes"] < MINIMUM_HTJ2K_CASES or any(counts[k] for k in counts if k != "successes"):
        raise ValueError(f"{name}: incomplete or failed HTJ2K selection: {counts}")
    names = {case.attrib.get("name") for case in root.findall("TestCase")}
    if not REQUIRED_CASES.issubset(names):
        raise ValueError(f"{name}: required cases not executed: {sorted(REQUIRED_CASES - names)}")
    return counts


def check_cxx14(run, binary, output, tracked, report):
    """The undefined shift must reproduce unsuppressed and only the tracked entry may hide it."""
    lines = {}
    for name, case in PROBES.items():
        probe = "unsuppressed-" + name
        code, log = run(probe, [binary, case], BARE_OPTIONS)
        lines[name] = reported_line(probe, code, log, "ojph_colour.cpp", NEGATIVE_SHIFT)
    if lines["generic32"] == lines["generic64"]:
        raise ValueError("generic 32-bit and 64-bit probes did not reach separate branches")
    report["reproduced_at_line"] = lines
    return run_htj2k_suite(run, binary, output, "tracked-htj2k", tracked)


def check_controls(run, bin_dir, tracked):
    """Tie both modes to the suppression's scope and the compiler's language rule."""
    control = bin_dir / "pacs_ubsan_shift_control"
    control_cxx20 = bin_dir / "pacs_ubsan_shift_control_cxx20"
    name = "unrelated-shift-cxx14"
    code, log = run(name, [control], tracked)
    reported_line(name, code, log, CONTROL_SOURCE, NEGATIVE_SHIFT)
    name = "language-mode-cxx20"
    code, log = run(name, [control_cxx20], BARE_OPTIONS)
    if code != 0 or "runtime error:" in log:
        raise ValueError(f"{name}: C++20 must define this shift; exit {code}, inspect {name}.log")
    name = "instrumentation-cxx20"
    code, log = run(name, [control_cxx20, "overflow"], BARE_OPTIONS)
    reported_line(name, code, log, CONTROL_SOURCE, SIGNED_OVERFLOW)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--openjph-standard", type=int, choices=sorted(COVERAGE), required=True)
    parser.add_argument("--suppression", type=Path,
                        default=Path(__file__).resolve().parents[1] / ".github/sanitizers/ubsan.supp")
    args = parser.parse_args(argv)
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"schema_version": 1, "architecture": platform.machine(),
              "openjph_standard": args.openjph_standard, "coverage": COVERAGE[args.openjph_standard],
              "upstream_issue": "https://github.com/aous72/OpenJPH/issues/406", "commands": {}}
    build_dir = args.build_dir.resolve()
    bin_dir = build_dir / "bin"
    binary = bin_dir / "encoding_tests"
    suppression = args.suppression.resolve()
    tracked = BARE_OPTIONS + ":suppressions=" + str(suppression)
    run = functools.partial(run_probe, args.output, report)

    try:
        if report["architecture"].lower() not in ("aarch64", "arm64"):
            raise ValueError("this required coverage must execute on native ARM64")
        verify_suppressions(suppression)
        report["translation_units"] = verify_build(build_dir, args.openjph_standard)
        if args.openjph_standard == 14:
            counts = check_cxx14(run, binary, args.output, tracked, report)
        else:
            counts = run_htj2k_suite(run, binary, args.output, "unsuppressed-htj2k", BARE_OPTIONS)
        report["htj2k_cases"] = counts
        check_controls(run, bin_dir, tracked)
        report["result"] = "passed: " + COVERAGE[args.openjph_standard]
        print(f"ARM HTJ2K C++{args.openjph_standard}: {counts['successes']} cases passed; "
              f"{COVERAGE[args.openjph_standard]}; controls verified")
        return 0
    except (KeyError, OSError, ValueError, ET.ParseError) as exc:
        report["result"] = "failed"
        report["error"] = str(exc)
        print(f"ARM HTJ2K sanitizer validation: {exc}", file=sys.stderr)
        return 1
    finally:
        (args.output / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())
