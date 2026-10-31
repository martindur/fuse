# Fuse in Neovim

Install the commands, Neovim integration, and syntax parser:

```sh
make PREFIX="$HOME/.local" install install-editor
```

Add the runtime and grammar to Neovim's runtime path in your configuration:

```lua
local data = vim.fn.expand("$HOME/.local/share/fuse")
vim.opt.runtimepath:append(data .. "/nvim")
vim.opt.runtimepath:append(data .. "/tree-sitter-fuse")
require("fuse.lsp").setup()
```

The runtime maps `.pgl`, `.ptex`, and `.pmesh` to `fusegl`, `fusetex`, and
`fusemesh`. All use the shared Fuse highlighter, while each file type starts
its own compiler's `lsp` command. Files are grouped by Git root where present,
otherwise by directory.

For checkout use, run `make tree-sitter-parser` and configure:

```lua
local repo = "/path/to/fuse"
vim.opt.runtimepath:append(repo .. "/editors/nvim")
vim.opt.runtimepath:append(repo .. "/tree-sitter-fuse")
vim.treesitter.language.add("fuse", {
  path = repo .. "/build/tree-sitter/fuse.so",
})
require("fuse.lsp").setup({
  commands = {
    fusegl = { repo .. "/build/fusegl", "lsp" },
    fusetex = { repo .. "/build/fusetex", "lsp" },
    fusemesh = { repo .. "/build/fusemesh", "lsp" },
  },
})
```

Command overrides can be supplied individually. This integration uses native
Neovim LSP and Tree-sitter facilities. Other editors can launch the same three
LSP commands and load the shared grammar through their own configuration.
