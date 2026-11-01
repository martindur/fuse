# Fuse in Neovim

Install Fuse, the Neovim integration, and syntax parser:

```sh
make PREFIX="$HOME/.local" install install-editor
```

Add `~/.local/bin` to your `PATH`, then configure Neovim:

```lua
local data = vim.fn.expand("$HOME/.local/share/fuse")
vim.opt.runtimepath:append(data .. "/nvim")
vim.opt.runtimepath:append(data .. "/tree-sitter-fuse")
require("fuse.lsp").setup()
```

The runtime maps `.pgl`, `.ptex`, and `.pmesh` to `fusegl`, `fusetex`, and
`fusemesh`. All use the Fuse highlighter and one `fuse lsp` client per project
root. The server selects the capability per document. Files are grouped by
Git root where present, otherwise by directory.

For checkout use, run `make tree-sitter-parser` and configure:

```lua
local repo = "/path/to/fuse"
vim.opt.runtimepath:append(repo .. "/editors/nvim")
vim.opt.runtimepath:append(repo .. "/tree-sitter-fuse")
vim.treesitter.language.add("fuse", {
  path = repo .. "/build/tree-sitter/fuse.so",
})
require("fuse.lsp").setup({ command = { repo .. "/build/fuse", "lsp" } })
```

Other editors can launch the same `fuse lsp` command and configure the three
language IDs or filename extensions. Selective builds serve only their
included capabilities.
