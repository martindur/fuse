# Changelog

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

