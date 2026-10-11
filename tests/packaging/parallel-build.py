#!/usr/bin/env python3
"""Exercise build ordering with real compilation and minimal X11-free sources."""
import os
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class ParallelBuildTests(unittest.TestCase):
    def test_rebuild_preserves_requested_binaries(self):
        for target in ("release", "strict", "debug"):
            with self.subTest(target=target), tempfile.TemporaryDirectory(
                prefix="cupidwm-parallel-build-"
            ) as tmp:
                work = pathlib.Path(tmp)
                (work / "Makefile").write_bytes((ROOT / "Makefile").read_bytes())
                (work / "config.mk").write_text(
                    "VERSION = regression\n"
                    "CFLAGS = -std=c99 ${EXTRA_CFLAGS}\n"
                    "LDFLAGS = ${EXTRA_LDFLAGS}\n"
                )
                (work / "config.h").touch()
                (work / "src").mkdir()
                (work / "tools").mkdir()
                (work / "src/defs.h").touch()
                for source in ("src/cupidwm.c", "tools/cupidwmctl.c"):
                    (work / source).write_text("int main(void) { return 0; }\n")
                for attempt in range(2):
                    result = subprocess.run(
                        ["make", "--no-print-directory", "-j8", target,
                         "CC=" + os.environ.get("CC", "cc")],
                        cwd=work, capture_output=True, text=True, timeout=30,
                    )
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertTrue((work / "cupidwm").is_file(),
                                    f"{target} removed WM on attempt {attempt}")
                    if target != "debug":
                        self.assertTrue((work / "cupidwmctl").is_file(),
                                        f"{target} removed CLI on attempt {attempt}")
                    if target in ("strict", "debug"):
                        self.assertIn("-fsanitize=address,undefined", result.stdout)


if __name__ == "__main__":
    unittest.main()
