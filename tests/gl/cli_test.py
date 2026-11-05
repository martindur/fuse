"""Shader command and catalog integration tests."""
from pathlib import Path
import re
import subprocess
import sys
import unittest
from tests.cli_support import CompilerTests, URI, opened, changed, at, notification, request

SOURCE = "surface.uv.center().circle(0.2).fill().colorize(rgb(1.0, 0.5, 0.0))"

class ShaderTests(CompilerTests):
    binary = Path("build/grafic").resolve()
    language_id = "graficgl"
    command = "gl"

    def test_vertex_result_contract_and_generated_interface(self):
        result = self.run_command("--stage", "vertex", source="vertex(position, uv)")
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b"layout(location = 0) in vec3 a_position", result.stdout)
        self.assertIn(b"GraficVertex graficResult = vertex(position, uv);", result.stdout)
        self.assertIn(b"u_projection * u_view * u_model * vec4(graficPosition, 1.0)", result.stdout)
        self.assertIn(b"graficUv = graficResult.uv", result.stdout)
        self.assertNotIn(b"fwidth", result.stdout)
        shown = self.run_command("--stage", "vertex", source="vertex(position, uv).show()")
        self.assertEqual(shown.stdout, result.stdout)
        for source in ("uv", "1.0", "vec4(0.0, 0.0, 0.0, 1.0)"):
            result = self.run_command("--stage", "vertex", source=source)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, b"")
        result = self.run_command("--stage", "vertex", "--preview-line", "2", source="position\n.x()")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"program result must be vertex", result.stderr)
        for args in (("--stage",), ("--stage", "geometry"), ("--stage", "vertex", "--stage", "fragment")):
            self.assertNotEqual(self.run_command(*args, source="position").returncode, 0)

    def test_fragment_normals_and_vector_lighting_catalog(self):
        result = self.run_command(source="vec4(surface.normal.multiply(0.5), 1.0)")
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b"cross(dFdx(point), dFdy(point))", result.stdout)
        self.assertIn(b"gl_FrontFacing", result.stdout)
        self.assertIn(b"GraficSurface surface =", result.stdout)
        for name in ("surface", "grafic_surface_normal"):
            result = self.run_command(source=f"let {name} = 1.0; vec4(surface.uv, 0.0, 1.0)")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"binding name is reserved", result.stderr)
        result = self.run_command("--stage", "vertex", source="surface.normal")
        self.assertNotEqual(result.returncode, 0)
        uri = "file:///water.gfrag"
        _, responses = self.lsp_session(
            opened("surface.normal.", self.language_id, uri),
            request("textDocument/completion", 2, at("surface.normal.", uri)),
            request("textDocument/hover", 3, at("surface.nor", uri)),
        )
        names = {item["label"] for item in responses[2]["result"]["items"]}
        self.assertTrue({"normalize", "dot", "multiply", "x", "y", "z"} <= names)
        self.assertIn("local normal", responses[3]["result"]["contents"]["value"])
        for source in ("vec4(surface.uv.normalize(), 0.0, 1.0)", "surface.normal.dot(1.0)"):
            self.assertNotEqual(self.run_command(source=source).returncode, 0)

    def test_fragment_position_catalog_and_reserved_varying(self):
        uri = "file:///water.gfrag"
        _, responses = self.lsp_session(
            opened("surface.position.", self.language_id, uri),
            request("textDocument/completion", 2, at("surface.position.", uri)),
            request("textDocument/hover", 3, at("surface.pos", uri)),
        )
        names = {item["label"] for item in responses[2]["result"]["items"]}
        self.assertTrue({"x", "y", "z", "show"} <= names)
        self.assertIn("Displaced mesh-local position", responses[3]["result"]["contents"]["value"])
        for stage, source in (("fragment", "let graficPosition = 1.0; vec4(surface.uv, 0.0, 1.0)"),
                              ("vertex", "let graficPosition = 1.0; surface.position")):
            result = self.run_command("--stage", stage, source=source)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"binding name is reserved", result.stderr)

    def test_structured_fields_aliases_and_screen_coordinates(self):
        for receiver, expected in (("surface.", {"position", "uv", "normal"}),
                                   ("screen.", {"position", "uv"}),
                                   ("let data = surface; data.", {"position", "uv", "normal"}),
                                   ("let output = vertex(position, uv); output.", {"position", "uv", "normal", "show"})):
            identity = "graficvert" if "vertex(" in receiver else self.language_id
            _, responses = self.lsp_session(
                opened(receiver, identity),
                request("textDocument/completion", 2, at(receiver)),
            )
            self.assertEqual({item["label"] for item in responses[2]["result"]["items"]}, expected)
        for source in ("vec4(uv, 0.0, 1.0)", "vec4(position, 1.0)", "vec4(normal, 1.0)",
                       "surface.missing", "screen.normal", "surface.uv.missing", "surface.uv()"):
            result = self.run_command(source=source)
            self.assertNotEqual(result.returncode, 0, source)
        result = self.run_command(source="let data = screen; vec4(data.uv, 0.0, 1.0)")
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        _, responses = self.lsp_session(
            opened("screen.position"), request("textDocument/hover", 2, at("screen.pos")),
        )
        self.assertIn("pixel-center", responses[2]["result"]["contents"]["value"])
        for source in ("vertex(position, uv)", "vertex(position, uv, vec3(0.0, 2.0, 0.0))",
                       "let output = vertex(position, uv); output"):
            self.assertEqual(self.run_command("--stage", "vertex", source=source).returncode, 0)
        for source in ("position", "vertex(position)", "vertex(uv, position)", "vertex(position, uv, 1.0)"):
            self.assertNotEqual(self.run_command("--stage", "vertex", source=source).returncode, 0)

    def test_vertex_catalog_and_lsp_are_stage_specific(self):
        for args in (("--stage", "vertex", "--builtins"), ("--builtins", "--stage", "vertex")):
            result = self.run_command(*args)
            self.assertEqual(result.returncode, 0)
            self.assertIn(b"position", result.stdout)
            self.assertNotIn(b"fill(", result.stdout)
        uri = "file:///waves.gvert"
        _, responses = self.lsp_session(
            opened("position.", None, uri),
            request("textDocument/completion", 2, at("position.", uri)),
        )
        names = {item["label"] for item in responses[2]["result"]["items"]}
        self.assertTrue({"x", "y", "z", "add", "multiply", "show"} <= names)
        self.assertNotIn("fill", names)
        packets, responses = self.lsp_session(
            opened("vertex(position, uv)", "graficvert", uri),
            request("textDocument/hover", 2, at("vertex(pos", uri)),
        )
        self.assertIn("local coordinates", responses[2]["result"]["contents"]["value"])
        self.assertTrue(any(p.get("params", {}).get("diagnostics") == [] for p in packets))

    def test_line_comments_preserve_shader_output(self):
        self.assert_comments_preserve_output(SOURCE)

    def test_show_is_identity_for_every_numeric_type(self):
        for source in ["0.25.show().colorize(rgb(1.0, 0.5, 0.0))",
                       "surface.uv.show().circle(0.2).fill().colorize(rgb(1.0, 0.5, 0.0))",
                       "0.25.colorize(rgb(1.0, 0.5, 0.0).show())",
                       SOURCE + ".show()"]:
            with self.subTest(source=source):
                result = self.run_command(source=source)
                self.assertEqual(result.returncode, 0, result.stderr.decode())
                self.assertEqual(result.stdout,
                                 self.run_command(source=source.replace(".show()", "")).stdout)
        self.assertIn("show", self.completions("surface.uv."))

    def test_comments_preserve_preview_lines_and_diagnostic_positions(self):
        source = "// shader\nsurface.uv // coordinates\n// centered preview follows\n.center() // preview"
        result = self.run_command("--preview-line", "4", source=source)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b"fragColor = vec4(center(surface.uv), 0.0, 1.0);", result.stdout)
        result = self.run_command("--preview-line", "3", source=source)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"line has no previewable expression", result.stderr)
        result = self.run_command(source="// shader\nsurface.uv // coordinates\n.missing()")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"semantic error at 3:2: unknown function", result.stderr)

    def test_completion_ignores_comments_and_preserves_receiver_after_comment(self):
        for source in ("// surface.uv.", SOURCE + " // surface.uv."):
            with self.subTest(source=source):
                self.assertEqual(self.completions(source), {})
        for source in ("surface.uv // surface.position\n.", "let p = surface.uv // surface.position\n."):
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
        self.assertIn(b"in vec2 graficUv;", result.stdout)
        self.assertIn(
            b"fragColor = colorize(fill(circle(center(surface.uv), 0.2)), rgb(1.0, 0.5, 0.0));",
            result.stdout,
        )

    def test_preview_line(self):
        source = "surface.uv\n.center()\n.circle(0.2)"
        result = self.run_command("--preview-line", "2", source=source)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b"fragColor = vec4(center(surface.uv), 0.0, 1.0);", result.stdout)
        result = self.run_command("--preview-line", "3", source=source)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b"fragColor = vec4(vec3(circle(center(surface.uv), 0.2)), 1.0);", result.stdout)

    def test_invalid_preview(self):
        for source, line in [(SOURCE, "20"), ("tex0", "1")]:
            with self.subTest(source=source, line=line):
                result = self.run_command("--preview-line", line, source=source)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")
                self.assertIn(b"grafic gl:", result.stderr)

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
                self.assertIn(b"Usage: grafic gl", result.stderr)

    def test_invalid_programs(self):
        for source, error in [
            ("", b"expected an expression"),
            ("surface.uv | center()", b"parse error"),
            ("surface.uv.center()", b"program result must be vec4"),
            ("surface.uv.noise()", b"unknown function"),
            ("disk()", b"unknown function"),
        ]:
            with self.subTest(source=source):
                result = self.run_command(source=source)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")
                self.assertIn(error, result.stderr)

    def test_help(self):
        for arguments in [["--help"], ["-h"]]:
            with self.subTest(arguments=arguments):
                result = self.run_command(*arguments)
                self.assertEqual(result.returncode, 0)
                self.assertIn(b"Usage: grafic gl", result.stdout)
                self.assertEqual(result.stderr, b"")

    def test_completion_matches_builtin_help(self):
        result = self.run_command("--builtins")
        self.assertEqual(result.returncode, 0)
        names = set(re.findall(r"^  ([a-zA-Z_]\w*)[(:]", result.stdout.decode(), re.MULTILINE))
        self.assertIn("surface", names)
        self.assertIn("screen", names)
        self.assertIn("sample", names)
        self.assertEqual(set(self.completions("")), names)

    def test_vec2_receiver(self):
        items = self.completions("surface.uv.")
        self.assertTrue({"center", "rotate", "sample", "mirror"} <= items.keys())
        self.assertTrue({"r", "fill", "noise", "disk"}.isdisjoint(items))

    def test_scalar_receiver(self):
        items = self.completions("surface.uv.center().circle(0.2).")
        self.assertTrue({"fill", "stroke", "colorize", "multiply"} <= items.keys())
        self.assertNotIn("sample", items)
        self.assertEqual(items["multiply"]["detail"], "(float, float) -> float")

    def test_sampled_color_receiver(self):
        items = self.completions("surface.uv.sample(tex0).")
        self.assertTrue({"r", "g", "b", "a", "multiply"} <= items.keys())
        self.assertEqual(items["multiply"]["detail"], "(vec4, vec4) -> vec4")
        self.assertNotIn("sample", items)

    def test_unfinished_binding_receiver(self):
        items = self.completions("let p = surface.uv.center().")
        self.assertIn("rotate", items)
        self.assertNotIn("colorize", items)

    def test_hover_uses_shader_documentation(self):
        _, responses = self.lsp_session(
            opened("surface.uv.sample(tex0)"), request("textDocument/hover", 2, at("surface.uv.sam")),
        )
        markdown = responses[2]["result"]["contents"]["value"]
        documentation = "Samples a texture at the input UV coordinate."
        self.assertIn("```graficgl", markdown)
        self.assertIn("sample(vec2, texture) -> vec4", markdown)
        self.assertIn(documentation, markdown)
        self.assertIn(documentation.encode(), self.run_command("--builtins").stdout)

    def test_document_diagnostics(self):
        packets, _ = self.lsp_session(
            opened("surface.uv.center()"), changed(SOURCE, 2), changed("surface.uv.noise()", 3),
            notification("textDocument/didClose", {"textDocument": {"uri": URI}}),
        )
        diagnostics = [packet["params"]["diagnostics"] for packet in packets
                       if packet.get("method") == "textDocument/publishDiagnostics"]
        self.assertEqual(len(diagnostics), 4)
        self.assertEqual(diagnostics[0][0]["source"], "graficgl")
        self.assertEqual(diagnostics[0][0]["message"], "program result must be vec4")
        self.assertEqual(diagnostics[1], [])
        self.assertEqual(diagnostics[2][0]["message"], "unknown function")
        self.assertEqual(diagnostics[3], [])

    @unittest.skipUnless(Path("/dev/full").exists(), "/dev/full is unavailable")
    def test_output_failure(self):
        with open("/dev/full", "wb") as output:
            result = subprocess.run(
                [str(self.binary), "gl"], input=SOURCE.encode(), stdout=output,
                stderr=subprocess.PIPE, timeout=10,
            )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"grafic gl: failed to", result.stderr)


if __name__ == "__main__":
    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        ShaderTests.binary = Path(sys.argv.pop(1)).resolve()
    unittest.main()
