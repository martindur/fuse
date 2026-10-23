# Fuse grammar in Neovim

Build the standalone grammar parser:

```sh
make tree-sitter-parser
```

Add the grammar directory to the runtime path and load the parser:

```lua
vim.opt.runtimepath:append("/path/to/fuse/tree-sitter-fuse")
vim.treesitter.language.add("fuse", {
  path = "/path/to/fuse/build/tree-sitter/fuse.so",
})
```

Products define their file extensions, file types, and LSP command. Once a
product has configured a file type, associate it with the shared grammar:

```lua
vim.treesitter.language.register("fuse", "your-product-filetype")
```

Start Tree-sitter for that file type using the editor's usual setup, or call
`vim.treesitter.start(0, "fuse")` in its buffer. The shared queries provide
syntax highlighting, indentation, and local binding information. Product
builtin knowledge is supplied through the product's LSP.

For an installed grammar, append the installed
`share/fuse/tree-sitter-fuse` directory to the runtime path. Its parser is at
`parser/fuse.so`.
