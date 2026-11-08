#!/usr/bin/env python3
"""Independent DCMTK/CLI checks; missing binaries are failures, never skips.

python3 test_recursive_anonymization.py BUILD_DIR [EVIDENCE_DIR]
Only synthetic identifiers are used. Command argv, exits, stdout, stderr and
Part 10 fixtures are retained in EVIDENCE_DIR (default BUILD_DIR/test-results/anonymization).
"""
import json
import shutil
import subprocess
import sys
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

CLASS = "1.2.840.10008.5.1.4.1.1.2"
STUDY, SERIES, IMAGE, OTHER = (f"1.2.826.0.1.3680043.10.543.1300.{i}" for i in range(1, 5))
IDENTIFIERS = ["SOURCE^PATIENT", "SOURCE-ID", "SOURCE-ACCESS", "19700101",
               "SOURCE-CREATOR", "SOURCE-PRIVATE", "SOURCE-PREAMBLE", "SOURCE-AE"]


def sequence(tag, content):
    return f"({tag}) SQ\n(fffe,e000) na\n{content}\n(fffe,e00d) na\n(fffe,e0dd) na\n"


def identifiers():
    return "\n".join([
        "(0010,0010) PN [SOURCE^PATIENT]", "(0010,0020) LO [SOURCE-ID]",
        "(0010,0030) DA [19700101]", "(0008,0050) SH [SOURCE-ACCESS]",
        "(0011,0010) LO [SOURCE-CREATOR]", "(0011,1010) LO [SOURCE-PRIVATE]",
        f"(0020,000d) UI [{STUDY}]", f"(0020,000e) UI [{SERIES}]"]) + "\n"


def fixture(instance=IMAGE):
    image = identifiers() + f"(0008,1150) UI [{CLASS}]\n(0008,1155) UI [{IMAGE}]\n"
    series = identifiers() + sequence("0008,1140", image)
    study = identifiers() + sequence("0008,1115", series)
    return (f"(0002,0016) AE [SOURCE-AE]\n(0008,0016) UI [{CLASS}]\n"
            f"(0008,0018) UI [{instance}]\n(0008,0060) CS [CT]\n" + identifiers()
            + sequence("0008,1110", study)
            + sequence("0008,2112", image)
            + sequence("0008,2218", "(0008,0100) SH [T-D3000]\n" + identifiers())
            + sequence("0010,1002", identifiers())  # X
            + sequence("0040,0513", identifiers())  # Z
            + sequence("0040,a730", identifiers())  # D
            + sequence("0040,0100", identifiers())  # No supported action: empty
            + sequence("7776,0010", identifiers())  # No dictionary entry: implicit UN guard
            + sequence("0040,0275", identifiers() + "(0040,1001) SH [SOURCE-PROC]\n")
            + "(0028,0002) US 1\n(0028,0004) CS [MONOCHROME2]\n"
              "(0028,0010) US 1\n(0028,0011) US 1\n(0028,0100) US 16\n"
              "(0028,0101) US 12\n(0028,0102) US 11\n(0028,0103) US 0\n"
              "(7fe0,0010) OW 002a\n")


class RecursiveAnonymization(unittest.TestCase):
    def setUp(self):
        self.root = EVIDENCE / self._testMethodName
        self.root.mkdir(parents=True, exist_ok=True)
        self.command_index = 0

    def command(self, *argv, expected=0):
        self.command_index += 1
        prefix = self.root / f"command-{self.command_index:02d}"
        result = subprocess.run(list(map(str, argv)), capture_output=True, text=True, timeout=30)
        prefix.with_suffix(".json").write_text(json.dumps(
            {"argv": list(map(str, argv)), "exit": result.returncode}, indent=2))
        prefix.with_suffix(".stdout").write_text(result.stdout)
        prefix.with_suffix(".stderr").write_text(result.stderr)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        return result

    def source(self, name="input", content=None, syntax="+te", lengths="+e", folder=None):
        folder = folder or self.root
        folder.mkdir(parents=True, exist_ok=True)
        dump = folder / (name + ".dump")
        dicom = folder / (name + ".dcm")
        dump.write_text(fixture() if content is None else content)
        self.command("dump2dcm", syntax, lengths, dump, dicom)
        raw = bytearray(dicom.read_bytes())
        raw[:128] = b"SOURCE-PREAMBLE".ljust(128, b"\0")
        dicom.write_bytes(raw)
        return dicom

    def inspect(self, output):
        result = self.command("dcm2xml", "+M", output)
        self.command("dcmdump", output)
        root = ET.fromstring(result.stdout)
        ds = root.find("data-set")
        self.assertIsNotNone(ds)
        return root, ds

    def value(self, ds, tag):
        node = ds.find(f"element[@tag='{tag}']")
        self.assertIsNotNone(node, tag)
        return node.text or ""

    def item(self, ds, tag):
        seq = ds.find(f"sequence[@tag='{tag}']")
        self.assertIsNotNone(seq, tag)
        items = seq.findall("item")
        self.assertEqual(len(items), 1, tag)
        return items[0]

    def clean_bytes(self, output, log, include_uids=True):
        needles = IDENTIFIERS + ["SOURCE-PROC"] + ([STUDY, SERIES, IMAGE, OTHER] if include_uids else [])
        for needle in needles:
            self.assertNotIn(needle.encode(), output.read_bytes(), needle)
            self.assertNotIn(needle, log.stdout + log.stderr, needle)
        self.assertEqual(output.read_bytes()[:128], bytes(128))

    def test_nested_references_all_length_and_vr_encodings(self):
        for syntax in ("+te", "+ti"):
            for lengths in ("+e", "-e"):
                with self.subTest(syntax=syntax, lengths=lengths):
                    source = self.source(syntax=syntax, lengths=lengths)
                    output = self.root / f"output{syntax}{lengths}.dcm"
                    result = self.command(BINARY, "--verbose", "--verify", "--detailed",
                                          "-k", "(0008,1110)", source, output)
                    self.clean_bytes(output, result)
                    root, ds = self.inspect(output)
                    study = self.item(ds, "0008,1110")
                    series = self.item(study, "0008,1115")
                    image = self.item(series, "0008,1140")
                    for level in (ds, study, series, image):
                        self.assertEqual(self.value(level, "0010,0010"), "ANONYMOUS")
                        self.assertEqual(self.value(level, "0010,0020"), "ANON_ID")
                        self.assertEqual(self.value(level, "0020,000d"), self.value(ds, "0020,000d"))
                        self.assertEqual(self.value(level, "0020,000e"), self.value(ds, "0020,000e"))
                    self.assertEqual(self.value(image, "0008,1155"), self.value(ds, "0008,0018"))
                    self.assertEqual(self.value(image, "0008,1150"), CLASS)
                    self.assertEqual(self.value(self.item(ds, "0008,2218"), "0008,0100"), "T-D3000")
                    self.assertEqual(self.value(root.find("meta-header"), "0002,0003"), self.value(ds, "0008,0018"))
                    self.assertEqual(self.value(root.find("meta-header"), "0002,0010"),
                                     "1.2.840.10008.1.2.1" if syntax == "+te" else "1.2.840.10008.1.2")
                    self.assertEqual(self.value(ds, "0028,0010"), "1")
                    self.assertNotIn("0011,", ET.tostring(ds, encoding="unicode"))

    def test_x_z_d_and_unknown_sequence_fallback(self):
        source = self.source()
        output = self.root / "output.dcm"
        result = self.command(BINARY, source, output)
        self.clean_bytes(output, result)
        _, ds = self.inspect(output)
        for tag in ("0010,1002", "0040,0275"):
            self.assertIsNone(ds.find(f"sequence[@tag='{tag}']"), tag)
        for tag in ("0040,0513", "0040,0100", "0008,1110"):
            seq = ds.find(f"sequence[@tag='{tag}']")
            self.assertIsNotNone(seq, tag)
            self.assertEqual(len(seq.findall("item")), 0, tag)
        self.assertEqual(len(self.item(ds, "0040,a730")), 0)

    def test_clean_descriptions_retains_clean_request_item(self):
        source = self.source()
        output = self.root / "output.dcm"
        result = self.command(BINARY, "--profile", "clean_descriptions", source, output)
        self.clean_bytes(output, result)
        _, ds = self.inspect(output)
        request = self.item(ds, "0040,0275")
        self.assertEqual(self.value(request, "0040,1001"), "")
        self.assertEqual(self.value(request, "0010,0020"), "ANON_ID")

    def test_batch_without_mapping_file_shares_uid_replacements(self):
        inputs = self.root / "inputs"
        self.source("first", fixture(), folder=inputs)
        self.source("second", fixture(OTHER), folder=inputs)
        outputs = self.root / "outputs"
        result = self.command(BINARY, "-o", outputs, inputs)
        self.assertEqual(len(list(outputs.glob("*.dcm"))), 2)
        first_path, second_path = outputs / "first.dcm", outputs / "second.dcm"
        self.clean_bytes(first_path, result)
        self.clean_bytes(second_path, result)
        _, first = self.inspect(first_path)
        _, second = self.inspect(second_path)
        self.assertEqual(self.value(first, "0020,000d"), self.value(second, "0020,000d"))
        self.assertEqual(self.value(first, "0020,000e"), self.value(second, "0020,000e"))
        self.assertNotEqual(self.value(first, "0008,0018"), self.value(second, "0008,0018"))
        self.assertEqual(self.value(self.item(second, "0008,2112"), "0008,1155"), self.value(first, "0008,0018"))

    def test_retain_uid_preserves_references_while_cleaning_items(self):
        source = self.source()
        output = self.root / "output.dcm"
        result = self.command(BINARY, "--retain-uid", "-k", "(0008,1110)", source, output)
        self.clean_bytes(output, result, include_uids=False)
        _, ds = self.inspect(output)
        image = self.item(self.item(self.item(ds, "0008,1110"), "0008,1115"), "0008,1140")
        self.assertEqual(self.value(ds, "0008,0018"), IMAGE)
        self.assertEqual(self.value(image, "0008,1155"), IMAGE)
        self.assertEqual(self.value(image, "0010,0010"), "ANONYMOUS")

    def test_diagnostics_do_not_echo_retained_identifiers(self):
        source = self.source()
        output = self.root / "output.dcm"
        result = self.command(BINARY, "-k", "PatientName", "--verbose", "--verify", source, output)
        self.assertIn("at depth 1", result.stdout)
        self.assertIn("may contain identifying information", result.stdout)
        for needle in IDENTIFIERS:
            self.assertNotIn(needle, result.stdout + result.stderr)
        _, ds = self.inspect(output)
        self.assertEqual(self.value(ds, "0010,0010"), "SOURCE^PATIENT")

    def test_failed_recursive_action_never_writes_output(self):
        nested = identifiers()
        for _ in range(65):
            nested = sequence("0008,2218", nested)
        source = self.source(content=f"(0008,0016) UI [{CLASS}]\n(0008,0018) UI [{IMAGE}]\n" + nested)
        output = self.root / "output.dcm"
        result = self.command(BINARY, source, output, expected=2)
        self.assertIn("failed actions; output was not saved", result.stderr)
        self.assertFalse(output.exists())
        self.assertIn(b"SOURCE^PATIENT", source.read_bytes())


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3):
        sys.exit("usage: test_recursive_anonymization.py BUILD_DIR [EVIDENCE_DIR]")
    build = Path(sys.argv[1]).resolve()
    BINARY = build / "bin/dcm_anonymize"
    EVIDENCE = Path(sys.argv[2]).resolve() if len(sys.argv) == 3 else build / "test-results/anonymization"
    for binary in (BINARY, "dump2dcm", "dcm2xml", "dcmdump"):
        if not shutil.which(str(binary)):
            sys.exit(f"required binary missing: {binary}")
    unittest.main(argv=[sys.argv[0]], verbosity=2)
