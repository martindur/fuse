# Grafic in Neovim

Install Grafic, the Neovim integration, and syntax parser:

```sh
make PREFIX="$HOME/.local" install install-editor
```

Add `~/.local/bin` to your `PATH`, then configure Neovim:

```lua
local data = vim.fn.expand("$HOME/.local/share/grafic")
vim.opt.runtimepath:append(data .. "/nvim")
vim.opt.runtimepath:append(data .. "/tree-sitter-grafic")
require("grafic.lsp").setup()
```

The runtime maps `.gfrag`, `.gvert`, `.gtex`, and `.gmesh` to `graficgl`,
`graficvert`, `grafictex`, and `graficmesh`. All use the Grafic highlighter and one `grafic lsp` client per project
root. The server selects the capability per document. Files are grouped by
Git root where present, otherwise by directory.

`show()` uses the `@function.builtin.grafic` highlight, linked to `Keyword` by
default. Override that group in your colorscheme to customize inspection markers.

For checkout use, run `make tree-sitter-parser` and configure:

```lua
local repo = "/path/to/grafic"
vim.opt.runtimepath:append(repo .. "/editors/nvim")
vim.opt.runtimepath:append(repo .. "/tree-sitter-grafic")
vim.treesitter.language.add("grafic", {
  path = repo .. "/build/tree-sitter/grafic.so",
})
require("grafic.lsp").setup({ command = { repo .. "/build/grafic", "lsp" } })
```

Other editors can launch the same `grafic lsp` command and configure these
language IDs or filename extensions. Selective builds serve only their
included capabilities.
