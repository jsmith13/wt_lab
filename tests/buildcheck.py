# SPDX-License-Identifier: GPL-3.0-only
"""Check WT Lab's build/packaging boundary without a toolchain or SDK.

    python tests/buildcheck.py
"""
import contextlib
import io
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import build as builder

SYMS = "\n".join(f"{address:08x} g O .bss 00000000 {symbol}" for symbol, address in (
    ("_start",builder.APP_XIP), ("_bss_end",0x01C09000), ("_pool_end",0x01C21000), ("_rt_end",0x01C00100)))


class BuildIdentityTests(unittest.TestCase):
    def setUp(self):
        self.stack = contextlib.ExitStack()
        self.addCleanup(self.stack.close)
        self.out = Path(self.stack.enter_context(tempfile.TemporaryDirectory()))
        self.stack.enter_context(mock.patch.multiple(
            builder, OUT=self.out, PRODUCT="WTLABDEV", VERSION=None, SDK_SHA256={}))
        self.loader = self.stack.enter_context(mock.patch.object(
            builder, "build_loader", return_value=b"loader"))
        self.app = self.stack.enter_context(mock.patch.object(
            builder,             "build_app", return_value=(b"app", SYMS, "", "")))
        self.stack.enter_context(mock.patch.object(builder, "check", side_effect=lambda *_: ([], [])))
        self.stack.enter_context(mock.patch.object(builder, "mmio_check", return_value=[]))
        self.stack.enter_context(mock.patch.object(
            builder.fm1pkg_make, "flash_image", return_value=b"flash"))
        self.package = self.stack.enter_context(mock.patch.object(
            builder.fm1pkg_make, "ufw", return_value=b"package"))
        self.stack.enter_context(mock.patch.object(builder.fm1pkg_make, "SDK", None))

    def run_build(self, *args):
        with mock.patch.object(sys, "argv", ["build.py", *args]), \
                contextlib.redirect_stdout(io.StringIO()), \
                contextlib.redirect_stderr(io.StringIO()):
            return builder.main()

    def test_check_only_builds_but_never_packages(self):
        self.assertEqual(self.run_build("--check-only"), 0)
        self.loader.assert_called_once()
        self.app.assert_called_once()
        self.package.assert_not_called()
        report = json.loads((self.out / "wtlab-build.json").read_text())
        self.assertFalse(report["installable_package"])
        self.assertEqual(report["artifacts"]["app"]["sha256"],hashlib.sha256(b"app").hexdigest())
        self.assertEqual(report["memory_bytes"],{"ram":4096,"pool":4096,"ram_text":256})
        self.assertIn("firmware/src/wtlab.c",report["source_sha256"])

    def test_packaging_requires_a_separate_valid_identity(self):
        for args in ((), ("--release", "1.0.0"), ("--product", "WTLABDEV"),
                     ("--product", "FM-1_970"), ("--product", "FM-1_979")):
            with self.subTest(args=args), self.assertRaises(SystemExit) as exc:
                self.run_build(*args)
            self.assertEqual(exc.exception.code, 2)
        self.app.assert_not_called()
        self.package.assert_not_called()

    def test_release_preserves_explicit_identity_and_full_version(self):
        self.assertEqual(self.run_build(
            "--product", "FM-1_926", "--release", "1.2.3-test"), 0)
        self.assertEqual(builder.VERSION, "1.2.3-test")
        self.package.assert_called_once_with(b"flash", b"loader", "FM-1_926")
        self.assertEqual((self.out / "wtlab-1.2.3-test.fwsc").read_bytes(), b"package")
        self.assertTrue(json.loads((self.out / "wtlab-build.json").read_text())["installable_package"])

    def test_failed_checks_cannot_produce_a_package(self):
        (self.out / "wtlab-build.json").write_text("stale previous success")
        with mock.patch.object(builder, "check", side_effect=lambda *_: (["image too big"], [])), \
                self.assertRaisesRegex(SystemExit, "checks failed"):
            self.run_build("--product", "FM-1_926")
        self.package.assert_not_called()
        self.assertEqual(list(self.out.iterdir()), [])

    def test_build_report_is_reproducible(self):
        self.run_build("--check-only")
        first = (self.out / "wtlab-build.json").read_bytes()
        self.run_build("--check-only")
        self.assertEqual(first,(self.out / "wtlab-build.json").read_bytes())


class ResourceTests(unittest.TestCase):
    def test_missing_and_overflowing_memory_regions_fail(self):
        image = bytes.fromhex("04818000")
        self.assertEqual(builder.check(image,SYMS,"","")[0],[])
        for symbol,base,limit in builder.MEMORY.values():
            for value in (base-4,base+limit+4):
                syms = SYMS.replace(next(line for line in SYMS.splitlines() if line.endswith(symbol)),
                                    f"{value:08x} g O .bss 00000000 {symbol}")
                with self.subTest(symbol=symbol,value=value):
                    self.assertTrue(builder.check(image,syms,"","")[0])
            syms = "\n".join(line for line in SYMS.splitlines() if not line.endswith(symbol))
            self.assertTrue(builder.check(image,syms,"","")[0])

if __name__ == "__main__":
    unittest.main()
