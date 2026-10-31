# Changelog fragments

For a user-visible change, add a file named `<short-description>.<category>.md`
in this directory. Categories are `added`, `changed`, `deprecated`, `removed`,
`fixed`, and `security`.

Write one paragraph on a single line describing the change for a user. For example,
`line-comments.added.md` could contain:

```text
Support // line comments in shader, texture, and geometry programs.
```

Use a unique filename per change. Internal refactoring and minor documentation
edits do not need fragments. Choose the version when preparing a release;
fragment categories do not determine the version bump.

Release preparation combines pending fragments into `CHANGELOG.md` and removes
the consumed files. Only fragments merged into `main` are included.
