"""Image output and texture-catalog integration tests."""
from pathlib import Path
import re
import struct
import sys
import unittest
from tests.cli_support import CompilerTests, opened, at, request

SOURCE = "uv.multiply(3.0).noise().colorize(rgb(0.2, 0.6, 1.0))"


class TextureTests(CompilerTests):
    binary = Path("build/fuse").resolve()
    language_id = "fusetex"
    command = "tex"

    def test_line_comments_preserve_image_output(self):
        self.assert_comments_preserve_output(SOURCE, "--width", "8", "--height", "4", "--seed", "42")

    def render(self, *arguments, source=SOURCE):
        return self.run_command("--width", "8", "--height", "4", *arguments, source=source)

    def test_png_dimensions_and_seed(self):
        first = self.render("--seed", "42")
        self.assertEqual(first.returncode, 0, first.stderr.decode())
        self.assertEqual(first.stderr, b"")
        self.assertEqual(first.stdout[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(struct.unpack(">II", first.stdout[16:24]), (8, 4))
        self.assertEqual(first.stdout, self.render("--seed", "42").stdout)
        self.assertNotEqual(first.stdout, self.render("--seed", "43").stdout)

    def test_preview(self):
        result = self.render("--preview-line", "2", source="uv\n.noise()")
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(result.stdout[:8], b"\x89PNG\r\n\x1a\n")

    def test_help_and_catalog(self):
        for arguments in [("--help",), ("-h",)]:
            result = self.run_command(*arguments)
            self.assertEqual(result.returncode, 0)
            self.assertIn(b"Usage: fuse tex", result.stdout)
        result = self.run_command("--builtins")
        self.assertEqual(result.returncode, 0)
        names = set(re.findall(r"^  ([a-zA-Z_]\w*)[(:]", result.stdout.decode(), re.MULTILINE))
        self.assertEqual(set(self.completions("")), names)
        self.assertIn("noise", names)
        self.assertTrue({"sample", "time", "disk"}.isdisjoint(names))

    def test_typed_completion_and_hover(self):
        items = self.completions("let p = uv.")
        self.assertTrue({"noise", "length", "dot"} <= items.keys())
        self.assertNotIn("colorize", items)
        items = self.completions("uv.noise().")
        self.assertIn("colorize", items)
        self.assertEqual(items["multiply"]["detail"], "(float, float) -> float")
        _, responses = self.lsp_session(
            opened("uv.noise()", self.language_id),
            request("textDocument/hover", 2, at("uv.no")),
        )
        text = responses[2]["result"]["contents"]["value"]
        self.assertIn("noise(vec2) -> float", text)
        self.assertIn("Perlin noise", text)
        self.assertIn("```fusetex", text)

    def test_diagnostics(self):
        packets, _ = self.lsp_session(opened("uv", self.language_id))
        diagnostic = next(packet["params"]["diagnostics"][0] for packet in packets
                          if packet.get("method") == "textDocument/publishDiagnostics")
        self.assertEqual(diagnostic["source"], self.language_id)
        self.assertEqual(diagnostic["message"], "program result must be vec4")

    def test_rejected_programs_and_options(self):
        for source in ["", "uv", "uv.sample(tex0)", "disk()"]:
            with self.subTest(source=source):
                result = self.render(source=source)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")
        for arguments in [("--width", "0"), ("--seed", "-1"), ("lsp", "--seed", "1")]:
            with self.subTest(arguments=arguments):
                result = self.run_command(*arguments, source=SOURCE)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")


if __name__ == "__main__":
    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        TextureTests.binary = Path(sys.argv.pop(1)).resolve()
    unittest.main()
