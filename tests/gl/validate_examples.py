"""Compile every example shader and validate the emitted GLSL."""
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binary = Path(sys.argv[1] if len(sys.argv) > 1 else "build/fusegl").resolve()
    root = Path(__file__).resolve().parents[2]
    sources = sorted((root / "examples").rglob("*.pgl"))
    with tempfile.TemporaryDirectory(prefix="fuse-shaders-") as directory:
        shader = Path(directory) / "example.frag"
        for source in sources:
            print(f"Validating {source.relative_to(root)}", flush=True)
            result = subprocess.run(
                [str(binary)], input=source.read_bytes(), capture_output=True,
                check=True, timeout=30,
            )
            shader.write_bytes(result.stdout)
            subprocess.run(
                ["glslangValidator", "-S", "frag", str(shader)], check=True, timeout=30,
            )
    print(f"Validated {len(sources)} shader examples")


if __name__ == "__main__":
    main()
