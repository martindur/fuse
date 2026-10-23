#include "fuse/ast_dump.h"

#include <stddef.h>
#include <stdio.h>

// use FILE *output = fopen("path", "w+");
// And probably fclose?
// Figure out who destroys?
// What if it fails to open?

static bool write_indent(FILE *output, size_t depth) {
  for (size_t index = 0; index < depth; index +=1) {
    if (fputs("  ", output) == EOF) {
      return false;
    }
  }

  return true;
}

static bool write_span(FILE *output, const char *source, SourceSpan span) {
  if (span.length == 0) {
    return true;
  }

  size_t written = fwrite(
      source + span.offset,
      1,
      span.length,
      output
  );

  return written == span.length;
}

static bool dump_expression(
    FILE *output,
    const Expr *expression,
    const char *source,
    size_t depth
) {
  if (expression == NULL) {
    return false;
  }

  if (!write_indent(output, depth)) {
    return false;
  }

  switch (expression->kind) {
    case EXPR_IDENTIFIER:
      if (fputs("(identifier \"", output) == EOF
          || !write_span(output, source, expression->span)
          || fputs("\")", output) == EOF) {
        return false;
      }

      return true;

    case EXPR_NUMBER:
      if (fputs("(number \"", output) == EOF
          || !write_span(output, source, expression->span)
          || fputs("\")", output) == EOF) {
        return false;
      }

      return true;

    case EXPR_CALL:
      if (fputs("(call \"", output) == EOF
          || !write_span(
            output,
            source,
            expression->as.call.name
          )
          || fputc('"', output) == EOF) {
        return false;
      }

      for (size_t index = 0;
          index < expression->as.call.arguments.count;
          index += 1) {
        if (fputc('\n', output) == EOF
            || !dump_expression(
              output,
              expression->as.call.arguments.items[index],
              source,
              depth + 1
            )) {
          return false;
        }
      }

      return fputc(')', output) != EOF;

    case EXPR_PIPELINE:
      if (fputs("(pipeline\n", output) == EOF
          || !dump_expression(
            output,
            expression->as.pipeline.input,
            source,
            depth + 1
          )
          || fputc('\n', output) == EOF
          || !dump_expression(
            output,
            expression->as.pipeline.stage,
            source,
            depth + 1
          )
          || fputc(')', output) == EOF) {
        return false;
      }

      return true;
  }

  return false;
}

bool ast_dump(
    FILE *output,
    const Expr *expression,
    const char *source
) {
  if (output == NULL
      || expression == NULL
      || source == NULL) {
    return false;
  }

  if (!dump_expression(output, expression, source, 0)) {
    return false;
  }

  return fputc('\n', output) != EOF;
}
