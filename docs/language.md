# Fuse language

Fuse programs consist of zero or more bindings followed by one result
expression. The syntax does not define a builtin vocabulary; a product's
catalog supplies functions, globals, documentation, and reserved names.

## Expressions and calls

Expressions are identifiers, numbers, calls, parenthesized expressions, and
chains. Calls accept positional arguments:

```text
shift(point, 0.5)
```

Dot calls pass the expression on the left as the first argument:

```text
point.shift(0.5).magnitude()
```

This lowers to `magnitude(shift(point, 0.5))`. Chaining changes call syntax;
it does not introduce methods or mutable object state. Calls used as
additional arguments may themselves contain chains.

Numbers are decimal integers or fractions, optionally preceded by `-`.
They have type `float`. Identifiers start with an ASCII letter or `_` and
continue with ASCII letters, digits, or `_`. Whitespace can separate stages,
so a chain can span multiple lines. Comments and infix operators are not
part of the current syntax.

## Bindings

Bindings use `let` and end with a semicolon. The final result expression
has no trailing semicolon:

```text
let p = point.shift(0.5);
let amount = p.magnitude();
amount.finish()
```

Bindings are visible to later bindings and the final result. Duplicate names,
forward references, and names reserved by the product catalog are rejected.
The current language has no reassignment or user-defined functions.

## Types and product catalogs

The shared type vocabulary is `float`, `vec2`, `vec3`, `vec4`, `texture`, and
`mesh`. Function signatures determine argument and result types. Overloads
are resolved by exact parameter count and types; there are no implicit
conversions. Products supply constructors as ordinary functions when needed.

Type checking does not execute calls. Texture and mesh representations,
numeric evaluation, generated code, and file output belong to the product.

`semantic_analyze` infers the actual final type. Each product's dialect
descriptor specifies the required type of a complete program. Tools that
inspect an intermediate expression can infer its type independently of that
complete-program rule.

## Source locations

AST spans reference bytes in the source buffer. Compiler source positions
are one-based. LSP positions are zero-based UTF-16 positions, converted by
the shared server.

Intermediate selection uses one-based source lines. It selects the last
expression point on the requested line, including the initial expression
and each chained stage. Selection and lowering preserve source spans for
diagnostics.
