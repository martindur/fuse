"""Compile example and fixture shaders and validate the emitted GLSL."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binary = Path(sys.argv[1] if len(sys.argv) > 1 else "build/grafic").resolve()
    root = Path(__file__).resolve().parents[2]
    sources = sorted(path for directory in (root / "examples", root / "tests/view/fixtures")
                     for suffix in ("*.gfrag", "*.gvert") for path in directory.rglob(suffix))
    with tempfile.TemporaryDirectory(prefix="grafic-shaders-") as directory:
        shader = Path(directory) / "example.frag"
        for source in sources:
            print(f"Validating {source.relative_to(root)}", flush=True)
            stage = "vertex" if source.suffix == ".gvert" else "fragment"
            result = subprocess.run(
                [str(binary), "gl", "--stage", stage], input=source.read_bytes(), capture_output=True,
                check=True, timeout=30,
            )
            shader.write_bytes(result.stdout)
            subprocess.run(
                ["glslangValidator", "-S", "vert" if stage == "vertex" else "frag", str(shader)], check=True, timeout=30,
            )
    print(f"Validated {len(sources)} example and fixture shaders")


if __name__ == "__main__":
    main()
