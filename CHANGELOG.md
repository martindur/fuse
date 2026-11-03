# Changelog

## [0.4.0] - 2026-11-03

### Changed

- Rename Fuse to Grafic, including the `grafic` CLI, C library and headers, language server, and editor integration. Source files now use `.gfrag`, `.gtex`, and `.gmesh`; update existing files and editor configurations to the new names. Generated shaders use `graficUv` as their UV input.

## [0.3.0] - 2026-11-02

### Added

- Preview `.fmesh` sources directly with live reload, a fitted front view, and UV colors. Press W to toggle wireframe or U for UV colors. Mesh `.show()` markers select intermediate geometry without changing normal OBJ export; failed edits preserve the last working mesh.
- Preview standalone shaders with fuse view, automatic source reload, pause/resume controls, intermediate-line selection, and recovery from invalid edits while the last working shader keeps running.
- Highlight `show()` inspection markers separately from other function calls in Neovim, using a dedicated Tree-sitter capture with keyword styling by default.
- Open `.ftex` files directly with `fuse view`, and switch between a shader and its supplied texture inputs with keys 1–5. Use `.show()` in shader or texture sources to select an intermediate viewer output while normal compilation preserves the full program. The last marker in each file wins; `--preview-line` overrides markers in the directly opened file.
- Bind procedural texture sources to `fuse view` with `--tex0` through `--tex3`. Shader and texture edits reload independently, preserving the last working result on failure. Textures default to 512 × 512 pixels and seed 42, with options to override both dimensions and seed.

### Changed

- Rename Fuse source extensions to `.fgl`, `.ftex`, and `.fmesh` for shaders, textures, and meshes. Update source files and editor configurations to use the new extensions; the previous Pipe extensions are no longer recognized.

## [0.2.0] - 2026-11-01

### Added

- Support // line comments throughout shader, texture, and geometry programs, with syntax highlighting and comment-aware editor tooling.

### Changed

- Use fuse lsp for mixed shader, texture, and geometry workspaces, with per-document diagnostics, completion, hover, and a shared Neovim client.
- Replace fusegl, fusetex, and fusemesh with fuse gl, fuse tex, and fuse mesh, installed together as one fuse command; source builds can select capabilities with CAPABILITIES.

## [0.1.0] - 2026-10-31

### Added

- Compile shader, texture, and geometry programs with fusegl, fusetex, and fusemesh, with language servers, Neovim integration, and a source installer.
- Prepare releases with make release VERSION=MAJOR.MINOR.PATCH, review consolidated changelog notes in a release PR, and publish automatically when it merges.

