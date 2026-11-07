"""Viewer argument and file errors that require no graphics display."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class ViewTests(unittest.TestCase):
    binary = Path("build/grafic").resolve()

    def run_view(self, *arguments):
        return subprocess.run([str(self.binary), "view", *arguments], capture_output=True, timeout=10)

    def test_help_requires_no_window(self):
        for option in ("--help", "-h"):
            result = self.run_view(option)
            self.assertEqual(result.returncode, 0)
            self.assertIn(b"Usage: grafic view", result.stdout)
            self.assertIn(b"Space", result.stdout)
            self.assertEqual(result.stderr, b"")

    def test_invalid_arguments(self):
        for arguments in [(), ("--unknown",), ("--width",), ("--width", "0"),
                          ("--height", "-1"), ("--width", "9999999999999999999999999"),
                          ("--height", "2147483648"), ("--preview-line", "0"),
                          ("--preview-line", "no"), ("one.gfrag", "two.gfrag"), ("--",),
                          ("--tex0",), ("--tex4", "a.gtex"), ("--tex0", "--tex1"),
                          ("--tex0", "a.gtex", "--tex0", "b.gtex"),
                          ("--texture-width", "0"), ("--texture-height", "2147483648"),
                          ("--blend",), ("--blend", "additive"), ("--blend", "--help"),
                          ("--seed", "-1"), ("--seed", "18446744073709551616")]:
            with self.subTest(arguments=arguments):
                result = self.run_view(*arguments)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")
                self.assertIn(b"Usage: grafic view", result.stderr)

    def test_texture_rejects_alpha_blending_before_window_creation(self):
        result = self.run_view("source.gtex", "--blend", "alpha")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"requires a shader or mesh preview", result.stderr)
        self.assertNotIn(b"window", result.stderr)

    def test_standalone_texture_rejects_shader_inputs(self):
        for source in ("source.gtex", "source.gmesh"):
            result = self.run_view(source, "--tex0", "input.gtex")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"--texN inputs require a shader file", result.stderr)
            self.assertNotIn(b"window", result.stderr)

    def test_invalid_scene_inputs_fail_before_opening_a_window(self):
        for args in (("waves.gvert", "--vert", "other.gvert"),
                     ("mesh.gmesh", "--mesh", "other.gmesh"),
                     ("water.gfrag", "--frag", "other.gfrag"),
                     ("waves.gvert", "--mesh", "wrong.gfrag"),
                     ("water.gtex", "--vert", "waves.gvert")):
            result = self.run_view(*args)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"conflicting input or invalid", result.stderr)
            self.assertNotIn(b"window", result.stderr)

    def test_unknown_and_legacy_extensions_are_rejected(self):
        for source in ("source.pgl", "source.ptex", "source.pmesh", "source.glsl", "source.txt"):
            result = self.run_view(source)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"unsupported source extension", result.stderr)
            self.assertNotIn(b"window", result.stderr)

    def test_missing_file_and_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            for path in (directory, str(Path(directory) / "missing.gfrag")):
                result = self.run_view(path)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b"grafic view:", result.stderr)
                self.assertEqual(result.stdout, b"")
                self.assertNotIn(b"window", result.stderr)

    def test_nul_source_is_rejected_before_window_creation(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "nul.gfrag"
            path.write_bytes(b"uv\x00junk")
            result = self.run_view(str(path))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"NUL byte", result.stderr)
            self.assertNotIn(b"window", result.stderr)

    def test_texture_read_errors_require_no_window(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "shader.gfrag"
            path.write_text("surface.uv.sample(tex0)")
            texture_directory = Path(directory) / "directory.gtex"
            texture_directory.mkdir()
            for texture in (Path(directory) / "missing.gtex", texture_directory):
                result = self.run_view(str(path), "--tex0", str(texture), "--seed", "0")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b"grafic view: tex0:", result.stderr)
                self.assertNotIn(b"Usage:", result.stderr)
                self.assertNotIn(b"window", result.stderr)


if __name__ == "__main__":
    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        ViewTests.binary = Path(sys.argv.pop(1)).resolve()
    unittest.main()
