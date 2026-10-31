"""Shader command and catalog integration tests."""
from pathlib import Path
import re
import subprocess
import sys
import unittest
from tests.cli_support import CompilerTests, URI, opened, changed, at, notification, request

SOURCE = "uv.center().circle(0.2).fill().colorize(rgb(1.0, 0.5, 0.0))"

class ShaderTests(CompilerTests):
    binary = Path("build/fusegl").resolve()
    language_id = "fusegl"

    def test_line_comments_preserve_shader_output(self):
        self.assert_comments_preserve_output(SOURCE)

    def test_comments_preserve_preview_lines_and_diagnostic_positions(self):
        source = "// shader\nuv // coordinates\n// centered preview follows\n.center() // preview"
        result = self.run_command("--preview-line", "4", source=source)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b"fragColor = vec4(center(uv), 0.0, 1.0);", result.stdout)
        result = self.run_command("--preview-line", "3", source=source)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"line has no previewable expression", result.stderr)
        result = self.run_command(source="// shader\nuv // coordinates\n.missing()")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"semantic error at 3:2: unknown function", result.stderr)

    def test_completion_ignores_comments_and_preserves_receiver_after_comment(self):
        for source in ("// uv.", SOURCE + " // uv."):
            with self.subTest(source=source):
                self.assertEqual(self.completions(source), {})
        for source in ("uv // position\n.", "let p = uv // position\n."):
            with self.subTest(source=source):
                items = self.completions(source)
                self.assertTrue({"center", "rotate", "sample"} <= items.keys())
                self.assertNotIn("fill", items)

    def test_hover_ignores_comments_and_diagnostics_accept_commented_programs(self):
        source = "// sample\n" + SOURCE
        packets, responses = self.lsp_session(
            opened(source), request("textDocument/hover", 2, at("// sam")),
        )
        self.assertIsNone(responses[2]["result"])
        diagnostics = [packet["params"]["diagnostics"] for packet in packets
                       if packet.get("method") == "textDocument/publishDiagnostics"]
        self.assertEqual(diagnostics, [[]])

    def test_compiler_filter(self):
        result = self.run_command(source=SOURCE)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(result.stderr, b"")
        self.assertTrue(result.stdout.startswith(b"#version 330 core\n"))
        self.assertIn(b"in vec2 fuseUv;", result.stdout)
        self.assertIn(
            b"fragColor = colorize(fill(circle(center(uv), 0.2)), rgb(1.0, 0.5, 0.0));",
            result.stdout,
        )

    def test_preview_line(self):
        source = "uv\n.center()\n.circle(0.2)"
        result = self.run_command("--preview-line", "2", source=source)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b"fragColor = vec4(center(uv), 0.0, 1.0);", result.stdout)
        result = self.run_command("--preview-line", "3", source=source)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b"fragColor = vec4(vec3(circle(center(uv), 0.2)), 1.0);", result.stdout)

    def test_invalid_preview(self):
        for source, line in [(SOURCE, "20"), ("tex0", "1")]:
            with self.subTest(source=source, line=line):
                result = self.run_command("--preview-line", line, source=source)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")
                self.assertIn(b"fusegl:", result.stderr)

    def test_invalid_arguments(self):
        for arguments in [
            ["--unknown"], ["fmt"], ["lsp", "--builtins"],
            ["--preview-line"], ["--preview-line", "0"],
            ["--preview-line", "-1"], ["--preview-line", "x"],
            ["--preview-line", "18446744073709551616"],
            ["--builtins", "--preview-line", "1"],
        ]:
            with self.subTest(arguments=arguments):
                result = self.run_command(*arguments, source=SOURCE)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")
                self.assertIn(b"Usage: fusegl", result.stderr)

    def test_invalid_programs(self):
        for source, error in [
            ("", b"expected an expression"),
            ("uv | center()", b"parse error"),
            ("uv.center()", b"program result must be vec4"),
            ("uv.noise()", b"unknown function"),
            ("disk()", b"unknown function"),
        ]:
            with self.subTest(source=source):
                result = self.run_command(source=source)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")
                self.assertIn(error, result.stderr)

    def test_help(self):
        for arguments in [["--help"], ["-h"], ["lsp", "--help"]]:
            with self.subTest(arguments=arguments):
                result = self.run_command(*arguments)
                self.assertEqual(result.returncode, 0)
                self.assertIn(b"fusegl lsp", result.stdout)
                self.assertEqual(result.stderr, b"")

    def test_completion_matches_builtin_help(self):
        result = self.run_command("--builtins")
        self.assertEqual(result.returncode, 0)
        names = set(re.findall(r"^  ([a-zA-Z_]\w*)[(:]", result.stdout.decode(), re.MULTILINE))
        self.assertIn("uv", names)
        self.assertIn("sample", names)
        self.assertEqual(set(self.completions("")), names)

    def test_vec2_receiver(self):
        items = self.completions("uv.")
        self.assertTrue({"center", "rotate", "sample", "mirror"} <= items.keys())
        self.assertTrue({"r", "fill", "noise", "disk"}.isdisjoint(items))

    def test_scalar_receiver(self):
        items = self.completions("uv.center().circle(0.2).")
        self.assertTrue({"fill", "stroke", "colorize", "multiply"} <= items.keys())
        self.assertNotIn("sample", items)
        self.assertEqual(items["multiply"]["detail"], "(float, float) -> float")

    def test_sampled_color_receiver(self):
        items = self.completions("uv.sample(tex0).")
        self.assertTrue({"r", "g", "b", "a", "multiply"} <= items.keys())
        self.assertEqual(items["multiply"]["detail"], "(vec4, vec4) -> vec4")
        self.assertNotIn("sample", items)

    def test_unfinished_binding_receiver(self):
        items = self.completions("let p = uv.center().")
        self.assertIn("rotate", items)
        self.assertNotIn("colorize", items)

    def test_hover_uses_shader_documentation(self):
        _, responses = self.lsp_session(
            opened("uv.sample(tex0)"), request("textDocument/hover", 2, at("uv.sam")),
        )
        markdown = responses[2]["result"]["contents"]["value"]
        documentation = "Samples a texture at the input UV coordinate."
        self.assertIn("```fusegl", markdown)
        self.assertIn("sample(vec2, texture) -> vec4", markdown)
        self.assertIn(documentation, markdown)
        self.assertIn(documentation.encode(), self.run_command("--builtins").stdout)

    def test_document_diagnostics(self):
        packets, _ = self.lsp_session(
            opened("uv.center()"), changed(SOURCE, 2), changed("uv.noise()", 3),
            notification("textDocument/didClose", {"textDocument": {"uri": URI}}),
        )
        diagnostics = [packet["params"]["diagnostics"] for packet in packets
                       if packet.get("method") == "textDocument/publishDiagnostics"]
        self.assertEqual(len(diagnostics), 4)
        self.assertEqual(diagnostics[0][0]["source"], "fusegl")
        self.assertEqual(diagnostics[0][0]["message"], "program result must be vec4")
        self.assertEqual(diagnostics[1], [])
        self.assertEqual(diagnostics[2][0]["message"], "unknown function")
        self.assertEqual(diagnostics[3], [])

    @unittest.skipUnless(Path("/dev/full").exists(), "/dev/full is unavailable")
    def test_output_failure(self):
        with open("/dev/full", "wb") as output:
            result = subprocess.run(
                [str(self.binary)], input=SOURCE.encode(), stdout=output,
                stderr=subprocess.PIPE, timeout=10,
            )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"fusegl: failed to", result.stderr)


if __name__ == "__main__":
    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        ShaderTests.binary = Path(sys.argv.pop(1)).resolve()
    unittest.main()
