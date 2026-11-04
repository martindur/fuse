"""OBJ output and geometry-catalog integration tests."""
from pathlib import Path
import re
import sys
import unittest
from tests.cli_support import CompilerTests, opened, at, request

SOURCE = "disk().radius(2.0).segments(8).rings(2)"


class MeshTests(CompilerTests):
    binary = Path("build/grafic").resolve()
    language_id = "graficmesh"
    command = "mesh"

    def test_grid_topology_uvs_and_winding(self):
        result = self.run_command(source="grid().size(4.0, 2.0).subdivisions(2, 1)")
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        lines = result.stdout.decode().splitlines()
        vertices = [tuple(map(float, line.split()[1:])) for line in lines if line.startswith("v ")]
        uvs = [tuple(map(float, line.split()[1:])) for line in lines if line.startswith("vt ")]
        faces = [[int(v.split("/")[0]) - 1 for v in line.split()[1:]] for line in lines if line.startswith("f ")]
        self.assertEqual(len(vertices), 6)
        self.assertEqual(len(faces), 4)
        self.assertEqual(vertices[0], (-2, 0, -1))
        self.assertEqual(vertices[-1], (2, 0, 1))
        self.assertEqual(uvs[0], (0, 0))
        self.assertEqual(uvs[-1], (1, 1))
        for face in faces:
            a, b, c = [vertices[index] for index in face]
            normal_y = (b[2] - a[2]) * (c[0] - a[0]) - (b[0] - a[0]) * (c[2] - a[2])
            self.assertGreater(normal_y, 0)

    def test_invalid_grid_operations_do_not_export_partial_meshes(self):
        for source in ("grid().size(0, 1)", "grid().subdivisions(1.5, 2)",
                       "grid().subdivisions(4097, 1)", "grid().radius(2)",
                       "grid().rings(2)", "grid().segments(8)", "grid().uv_radial()", "disk().size(1, 1)"):
            result = self.run_command(source=source)
            self.assertNotEqual(result.returncode, 0, source)
            self.assertEqual(result.stdout, b"")

    def test_line_comments_preserve_mesh_output(self):
        self.assert_comments_preserve_output(SOURCE)

    def test_obj_output(self):
        result = self.run_command(source=SOURCE)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(result.stderr, b"")
        lines = result.stdout.decode().splitlines()
        self.assertEqual(sum(line.startswith("v ") for line in lines), 17)
        self.assertEqual(sum(line.startswith("vt ") for line in lines), 17)
        self.assertEqual(sum(line.startswith("f ") for line in lines), 24)
        self.assertTrue(all("/" in line for line in lines if line.startswith("f ")))

    def test_preview(self):
        result = self.run_command("--preview-line", "2", source="disk()\n.radius(2.0)\n.segments(8)")
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertIn(b"v ", result.stdout)

    def test_show_preserves_exported_mesh_and_has_lsp_support(self):
        source = "disk().segments(8).show().rings(2)"
        result = self.run_command(source=source)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(result.stdout, self.run_command(source=source.replace(".show()", "")).stdout)
        packets, responses = self.lsp_session(
            opened(source, self.language_id), request("textDocument/hover", 2, at("disk().segments(8).sh")),
        )
        self.assertIn("show(mesh) -> mesh", responses[2]["result"]["contents"]["value"])
        self.assertTrue(any(packet.get("params", {}).get("diagnostics") == [] for packet in packets))

    def test_help_and_catalog(self):
        for arguments in [("--help",), ("-h",)]:
            result = self.run_command(*arguments)
            self.assertEqual(result.returncode, 0)
            self.assertIn(b"Usage: grafic mesh", result.stdout)
        result = self.run_command("--builtins")
        names = set(re.findall(r"^  ([a-zA-Z_]\w*)[(:]", result.stdout.decode(), re.MULTILINE))
        self.assertEqual(set(self.completions("")), names)
        self.assertEqual(names, {"disk", "grid", "size", "subdivisions", "radius", "segments", "rings", "uv_radial", "show"})

    def test_binding_completion_without_globals(self):
        items = self.completions("let m = disk().")
        self.assertEqual(set(items), {"size", "subdivisions", "radius", "segments", "rings", "uv_radial", "show"})

    def test_hover_and_diagnostics(self):
        packets, responses = self.lsp_session(
            opened("disk()", self.language_id), request("textDocument/hover", 2, at("di")),
        )
        text = responses[2]["result"]["contents"]["value"]
        self.assertIn("disk() -> mesh", text)
        self.assertIn("filled disk", text)
        self.assertIn("```graficmesh", text)
        self.assertTrue(any(packet.get("params", {}).get("diagnostics") == [] for packet in packets))
        packets, _ = self.lsp_session(opened("1.0", self.language_id))
        diagnostic = next(packet["params"]["diagnostics"][0] for packet in packets
                          if packet.get("method") == "textDocument/publishDiagnostics")
        self.assertEqual(diagnostic["source"], self.language_id)
        self.assertEqual(diagnostic["message"], "program result must be mesh")

    def test_rejected_programs_and_options(self):
        for source in ["", "uv", "1.0", "noise()"]:
            with self.subTest(source=source):
                result = self.run_command(source=source)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")
        for arguments in [("--preview-line", "0"), ("--seed", "1"), ("lsp", "--preview-line", "1")]:
            with self.subTest(arguments=arguments):
                result = self.run_command(*arguments, source=SOURCE)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")


if __name__ == "__main__":
    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        MeshTests.binary = Path(sys.argv.pop(1)).resolve()
    unittest.main()
