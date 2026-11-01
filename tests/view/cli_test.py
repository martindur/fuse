"""Viewer argument and file errors that require no graphics display."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class ViewTests(unittest.TestCase):
    binary = Path("build/fuse").resolve()

    def run_view(self, *arguments):
        return subprocess.run([str(self.binary), "view", *arguments], capture_output=True, timeout=10)

    def test_help_requires_no_window(self):
        for option in ("--help", "-h"):
            result = self.run_view(option)
            self.assertEqual(result.returncode, 0)
            self.assertIn(b"Usage: fuse view", result.stdout)
            self.assertIn(b"Space", result.stdout)
            self.assertEqual(result.stderr, b"")

    def test_invalid_arguments(self):
        for arguments in [(), ("--unknown",), ("--width",), ("--width", "0"),
                          ("--height", "-1"), ("--width", "9999999999999999999999999"),
                          ("--height", "2147483648"), ("--preview-line", "0"),
                          ("--preview-line", "no"), ("one.pgl", "two.pgl"), ("--",)]:
            with self.subTest(arguments=arguments):
                result = self.run_view(*arguments)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")
                self.assertIn(b"Usage: fuse view", result.stderr)

    def test_missing_file_and_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            for path in (directory, str(Path(directory) / "missing.pgl")):
                result = self.run_view(path)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b"fuse view:", result.stderr)
                self.assertEqual(result.stdout, b"")
                self.assertNotIn(b"window", result.stderr)

    def test_nul_source_is_rejected_before_window_creation(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "nul.pgl"
            path.write_bytes(b"uv\x00junk")
            result = self.run_view(str(path))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"NUL byte", result.stderr)
            self.assertNotIn(b"window", result.stderr)


if __name__ == "__main__":
    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        ViewTests.binary = Path(sys.argv.pop(1)).resolve()
    unittest.main()
