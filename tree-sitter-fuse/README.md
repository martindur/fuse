# tree-sitter-fuse

This grammar covers Fuse bindings, calls, numbers, and chained expressions.
For example, `point.shift(0.5).magnitude()` chains function applications.
Capabilities define their builtin vocabulary and file associations separately.

`queries/fuse` contains generic highlighting, indentation, and local binding
queries. Editors can use these files through their usual Tree-sitter setup.

Generate and test from this directory:

```sh
tree-sitter generate
tree-sitter test
```

The root `make tree-sitter-parser` target builds the checked-in generated parser
as `build/tree-sitter/fuse.so`, exporting `tree_sitter_fuse`. Native builds do
not require the Tree-sitter CLI. The root `make tree-sitter-test` target runs
the corpus tests.
