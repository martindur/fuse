"""OBJ output and geometry-catalog integration tests."""
from pathlib import Path
import re
import math
import sys
import unittest
from tests.cli_support import CompilerTests, opened, at, request

SOURCE = "disk().radius(2.0).segments(8).rings(2)"


class MeshTests(CompilerTests):
    binary = Path("build/grafic").resolve()
    language_id = "graficmesh"
    command = "mesh"

    def test_scatter_slope_uvs_variation_and_seed(self):
        source = ("let ground = grid().size(2, 2).subdivisions(1, 1).rotate(-0.15, 0, 0.2); "
                  "strip().size(0.06, 0.5).segments(8).taper(1).scatter(ground, 240).seed(42)"
                  ".scale_variation(0.75, 1.25)")
        result = self.run_command(source=source)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(result.stdout, self.run_command(source=source).stdout)
        self.assertNotEqual(result.stdout, self.run_command(source=source.replace("seed(42)", "seed(43)")).stdout)
        lines = result.stdout.decode().splitlines()
        vertices = [tuple(map(float, line.split()[1:])) for line in lines if line.startswith("v ")]
        uvs = [tuple(map(float, line.split()[1:])) for line in lines if line.startswith("vt ")]
        self.assertEqual(len(vertices), 17 * 240)
        self.assertEqual(sum(line.startswith("f ") for line in lines), 15 * 240)
        heights, yaws = [], set()
        for copy in range(240):
            a, b = vertices[copy * 17:copy * 17 + 2]
            root = tuple((x+y)*0.5 for x, y in zip(a, b))
            # Undo Z then X: every copy origin must lie on the target grid.
            x = root[0]*math.cos(0.2) + root[1]*math.sin(0.2)
            y = -root[0]*math.sin(0.2) + root[1]*math.cos(0.2)
            z = root[2]
            local_y = y*math.cos(-0.15) + z*math.sin(-0.15)
            local_z = -y*math.sin(-0.15) + z*math.cos(-0.15)
            self.assertAlmostEqual(local_y, 0, places=6)
            self.assertTrue(-1.000001 <= x <= 1.000001 and -1.000001 <= local_z <= 1.000001)
            tip = vertices[copy * 17 + 16]
            self.assertAlmostEqual(tip[0], root[0], places=6)
            self.assertAlmostEqual(tip[2], root[2], places=6)
            height = tip[1] - root[1]
            self.assertTrue(0.374999 <= height <= 0.625001)
            heights.append(height)
            yaws.add(round(math.atan2(b[2]-a[2], b[0]-a[0]), 2))
            self.assertEqual(uvs[copy * 17:copy * 17 + 2], [(0, 0), (1, 0)])
            self.assertEqual(uvs[copy * 17 + 16], (0.5, 1.0))
        self.assertLess(min(heights), 0.42)
        self.assertGreater(max(heights), 0.58)
        self.assertGreater(len(yaws), 100)

    def test_mesh_recipe_aliases_and_validation(self):
        base = "strip().size(0.1, 0.5).segments(2).taper(1).scatter(grid().subdivisions(1, 1), 4).seed(42)"
        aliased = f"let patch = {base}; let other = patch.seed(43).scale_variation(2, 3); patch"
        self.assertEqual(self.run_command(source=base).stdout, self.run_command(source=aliased).stdout)
        self.assertEqual(self.run_command(source="strip()").stdout,
                         self.run_command(source="let a = strip(); let b = a.size(2, 3); a").stdout)
        rotated = self.run_command(source="strip().size(2, 1).segments(1).rotate(0, 0, 1.57079632679)")
        self.assertEqual(rotated.returncode, 0, rotated.stderr.decode())
        vertices = [tuple(map(float, line.split()[1:])) for line in rotated.stdout.decode().splitlines() if line.startswith("v ")]
        self.assertAlmostEqual(vertices[0][0], 0, places=6)
        self.assertAlmostEqual(vertices[0][1], -1, places=6)
        for operation in ("scatter(grid(), 0)", "scatter(grid(), 1.5)", "scatter(grid(), 1000001)",
                          "seed(1)", "scale_variation(1, 2)", "rotate(0, 0, 0).size(2, 2)",
                          "scatter(grid(), 2).seed(-1)", "scatter(grid(), 2).seed(16777216)",
                          "scatter(grid(), 2).scale_variation(0, 1)",
                          "scatter(grid(), 2).scale_variation(2, 1)",
                          "segments(4096).scatter(grid(), 1000000)"):
            with self.subTest(operation=operation):
                bad = self.run_command(source="strip()." + operation)
                self.assertNotEqual(bad.returncode, 0)
                self.assertEqual(bad.stdout, b"")
        self.assertIn("scatter", self.completions("strip()."))
        self.assertIn("seed", self.completions("strip().scatter(grid(), 2)."))

    def test_shared_recipe_inputs_are_built_once(self):
        source = "let shape0 = strip().segments(1).taper(1); "
        for index in range(1, 24):
            source += f"let shape{index} = shape{index-1}.scatter(shape{index-1}, 1); "
        result = self.run_command(source=source + "shape23")
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(sum(line.startswith(b"v ") for line in result.stdout.splitlines()), 3)

    def test_strip_topology_winding_and_uvs(self):
        for segments in (1, 5):
            for taper in (0.0, 0.4, 1.0):
                with self.subTest(segments=segments, taper=taper):
                    source = f"strip().size(0.2, 0.8).segments({segments}).taper({taper})"
                    result = self.run_command(source=source)
                    self.assertEqual(result.returncode, 0, result.stderr.decode())
                    lines = result.stdout.decode().splitlines()
                    vertices = [tuple(map(float, line.split()[1:])) for line in lines if line.startswith("v ")]
                    uvs = [tuple(map(float, line.split()[1:])) for line in lines if line.startswith("vt ")]
                    faces = [tuple(int(part.split("/")[0]) - 1 for part in line.split()[1:])
                             for line in lines if line.startswith("f ")]
                    self.assertEqual(len(vertices), 2 * (segments + 1) - (taper == 1.0))
                    self.assertEqual(len(uvs), len(vertices))
                    self.assertEqual(len(faces), 2 * segments - (taper == 1.0))
                    self.assertEqual(uvs[:2], [(0.0, 0.0), (1.0, 0.0)])
                    for position, uv in zip(vertices, uvs):
                        self.assertEqual(position[2], 0.0)
                        self.assertAlmostEqual(position[1], uv[1] * 0.8)
                        self.assertTrue(0 <= uv[0] <= 1 and 0 <= uv[1] <= 1)
                    area = 0.0
                    for face in faces:
                        self.assertTrue(all(0 <= index < len(vertices) for index in face))
                        a, b, c = [vertices[index] for index in face]
                        signed_area = ((b[0]-a[0])*(c[1]-a[1]) - (b[1]-a[1])*(c[0]-a[0])) * 0.5
                        self.assertGreater(signed_area, 0.0)
                        area += signed_area
                    self.assertAlmostEqual(area, 0.2 * 0.8 * (1 - taper * 0.5))
                    if taper == 1.0:
                        self.assertEqual(uvs[-1], (0.5, 1.0))
                        self.assertEqual(sum(uv[1] == 1 for uv in uvs), 1)
        self.assertEqual(self.run_command(source="strip().segments(4096).taper(1.0)").returncode, 0)

    def test_strip_validation_and_catalog(self):
        for source in ("strip().size(0.0, 1.0)", "strip().size(1.0, -1.0)",
                       "strip().segments(0)", "strip().segments(1.5)", "strip().segments(4097)",
                       "strip().taper(-0.1)", "strip().taper(1.1)",
                       "grid().taper(0.5)", "disk().taper(0.5)", "strip().rings(3)",
                       "strip().subdivisions(2, 2)"):
            with self.subTest(source=source):
                result = self.run_command(source=source)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")
        self.assertIn("strip", self.completions(""))
        self.assertIn("taper", self.completions("strip()."))
        _, responses = self.lsp_session(
            opened("strip()", self.language_id),
            request("textDocument/hover", 2, at("str")),
        )
        self.assertIn("rooted at Y=0", responses[2]["result"]["contents"]["value"])
        source = "strip().segments(5).show().taper(1.0)"
        self.assertEqual(self.run_command(source=source).stdout,
                         self.run_command(source=source.replace(".show()", "")).stdout)

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
        self.assertEqual(names, {"disk", "grid", "strip", "taper", "size", "subdivisions", "radius", "segments", "rings", "uv_radial", "show", "rotate", "scatter", "seed", "scale_variation"})

    def test_binding_completion_without_globals(self):
        items = self.completions("let m = disk().")
        self.assertEqual(set(items), {"size", "subdivisions", "radius", "segments", "rings", "uv_radial", "taper", "show", "rotate", "scatter", "seed", "scale_variation"})

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
