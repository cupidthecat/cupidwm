#!/usr/bin/env python3
"""Check that the real distcheck recipe rejects incomplete release archives."""
import io
import os
import pathlib
import subprocess
import tarfile
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
REQUIRED = (
    "Makefile", "README.md", "SECURITY.md", "THIRD_PARTY_NOTICE.md",
    "cupidwmctl.1", "docs/configuration.md", "docs/ecosystem.md",
    "src/cupidwm.c", "scripts/smoke-xephyr.sh", "tests/ewmh/invariants.sh",
    "tests/packaging/sanity.sh", "completions/cupidwmctl.bash",
    "examples/xinitrc", "packaging/arch/PKGBUILD", "tests/ipc/roundtrip.sh",
    "tools/cupidwmctl.c",
)


class DistcheckTests(unittest.TestCase):
    def check_archive(self, omitted=None, invalid=None):
        with tempfile.TemporaryDirectory(prefix="cupidwm-distcheck-") as tmp:
            work = pathlib.Path(tmp)
            (work / "Makefile").write_bytes((ROOT / "Makefile").read_bytes())
            (work / "config.mk").write_text("VERSION = regression\n")
            archive = work / "cupidwm-regression.tar.gz"
            if invalid == "corrupt":
                archive.write_bytes(b"this is not an archive")
            elif invalid != "missing":
                with tarfile.open(archive, "w:gz") as tar:
                    for name in REQUIRED:
                        if name == omitted:
                            continue
                        data = b"fixture\n"
                        entry = tarfile.TarInfo("cupidwm-regression/" + name)
                        entry.size = len(data)
                        tar.addfile(entry, io.BytesIO(data))
            extract_root = work / "extractions"
            extract_root.mkdir()
            # Suppress archive generation to exercise validation independently.
            result = subprocess.run(
                ["make", "--no-print-directory", "-o", "dist", "distcheck"],
                cwd=work, capture_output=True, text=True, timeout=10,
                env={**os.environ, "TMPDIR": str(extract_root)},
            )
            self.assertEqual(list(extract_root.iterdir()), [], "extraction directory leaked")
            if omitted or invalid:
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertNotIn("distcheck passed", result.stdout)
            else:
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn("distcheck passed", result.stdout)

    def test_complete_archive(self):
        self.check_archive()

    def test_missing_archive(self):
        self.check_archive(invalid="missing")

    def test_corrupt_archive(self):
        self.check_archive(invalid="corrupt")

    def test_each_required_file(self):
        for name in REQUIRED:
            with self.subTest(name=name):
                self.check_archive(omitted=name)


if __name__ == "__main__":
    unittest.main()
