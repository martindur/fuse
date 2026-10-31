#include "fuse/parser.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *source;
  const char *message;
  size_t offset;
  size_t length;
} FailureCase;

static bool span_equals(
    const char *source,
    SourceSpan span,
    const char *expected
) {
  size_t expected_length = strlen(expected);

  return span.length == expected_length
    && memcmp(
        source + span.offset,
        expected,
        expected_length
    ) == 0;
}

static void report_failure(
    const char *test_name,
    const char *message
) {
  fprintf(stderr, "FAIL %s: %s\n", test_name, message);
}

static bool test_identifier(void) {
  const char *test_name = "identifier";
  const char *source = "uv";

  Expr *root = NULL;
  ParseError error;

  if (!parse_source(
        source,
        strlen(source),
        &root,
        &error
    )) {
    fprintf(
        stderr,
        "FAIL %s: parse error at %zu:%zu: %s\n",
        test_name,
        error.span.offset,
        error.span.length,
        error.message
    );

    return false;
  }

  bool passed = true;

  if (root->kind != EXPR_IDENTIFIER) {
    report_failure(test_name, "expected identifier root");
    passed = false;
    goto cleanup;
  }

  if (!span_equals(source, root->span, "uv")) {
    report_failure(test_name, "incorrect identifier span");
    passed = false;
  }

cleanup:
  expr_destroy(root);

  if (passed) {
    printf("PASS %s\n", test_name);
  }

  return passed;
}

static bool test_call(void) {
  const char *test_name = "call";
  const char *source = "scale(2.5, time)";

  Expr *root = NULL;
  ParseError error;

  if (!parse_source(
        source,
        strlen(source),
        &root,
        &error
    )) {
    fprintf(
        stderr,
        "FAIL %s: parse error at %zu:%zu: %s\n",
        test_name,
        error.span.offset,
        error.span.length,
        error.message
    );

    return false;
  }

  bool passed = true;

  if (root->kind != EXPR_CALL) {
    report_failure(test_name, "expected call root");
    passed = false;
    goto cleanup;
  }

  if (!span_equals(source, root->span, source)) {
    report_failure(test_name, "incorrect call span");
    passed = false;
  }

  if (!span_equals(source, root->as.call.name, "scale")) {
    report_failure(test_name, "incorrect call name");
    passed = false;
  }

  if (root->as.call.arguments.count != 2) {
    report_failure(test_name, "expected two arguments");
    passed = false;
    goto cleanup;
  }

  Expr *first = root->as.call.arguments.items[0];
  Expr *second = root->as.call.arguments.items[1];

  if (first->kind != EXPR_NUMBER
      || !span_equals(source, first->span, "2.5")) {
    report_failure(test_name, "incorrect first argument");
    passed = false;
  }

  if (second->kind != EXPR_IDENTIFIER
      || !span_equals(source, second->span, "time")) {
    report_failure(test_name, "incorrect second argument");
    passed = false;
  }

cleanup:
  expr_destroy(root);

  if (passed) {
    printf("PASS %s\n", test_name);
  }

  return passed;
}

static bool test_pipeline(void) {
  const char *test_name = "pipeline";
  const char *source = "uv.center().rotate(time)";

  Expr *root = NULL;
  ParseError error;

  if (!parse_source(
        source,
        strlen(source),
        &root,
        &error
    )) {
    fprintf(
        stderr,
        "FAIL %s: parse error at %zu%zu: %s\n",
        test_name,
        error.span.offset,
        error.span.length,
        error.message
    );

    return false;
  }

  bool passed = true;

  if (root->kind != EXPR_PIPELINE) {
    report_failure(test_name, "expected outer pipeline");
    passed = false;
    goto cleanup;
  }

  Expr *inner = root->as.pipeline.input;
  Expr *rotate = root->as.pipeline.stage;

  if (inner == NULL || inner->kind != EXPR_PIPELINE) {
    report_failure(test_name, "expected inner pipeline");
    passed = false;
    goto cleanup;
  }

  if (rotate == NULL || rotate->kind != EXPR_CALL) {
    report_failure(test_name, "expected rotate call");
    passed = false;
    goto cleanup;
  }

  Expr *uv = inner->as.pipeline.input;
  Expr *center = inner->as.pipeline.stage;

  if (uv == NULL
      || uv->kind != EXPR_IDENTIFIER
      || !span_equals(source, uv->span, "uv")) {
    report_failure(test_name, "incorrect pipeline input");
    passed = false;
  }

  if (center == NULL
      || center->kind != EXPR_CALL
      || !span_equals(source, center->as.call.name, "center")
      || center->as.call.arguments.count != 0) {
    report_failure(test_name, "incorrect center stage");
    passed = false;
  }

  if (!span_equals(source, rotate->as.call.name, "rotate")) {
    report_failure(test_name, "incorrect rotate stage");
    passed = false;
  }

  if (rotate->as.call.arguments.count != 1) {
    report_failure(
        test_name,
        "expected one rotate argument"
    );
    passed = false;
    goto cleanup;
  }

  Expr *time = rotate->as.call.arguments.items[0];

  if (time->kind != EXPR_IDENTIFIER
      || !span_equals(source, time->span, "time")) {
    report_failure(test_name, "incorrect rotate argument");
    passed = false;
  }

cleanup:
  expr_destroy(root);

  if (passed) {
    printf("PASS %s\n", test_name);
  }

  return passed;
}

static bool test_program(void) {
  const char *test_name = "program with binding";
  const char *source =
      "let p = uv.center();\n"
      "p.vec4(0.0, 1.0)";

  Program *program = NULL;
  ParseError error;

  if (!parse_program(
        source,
        strlen(source),
        &program,
        &error
      )) {
    fprintf(
        stderr,
        "FAIL %s: parse error at %zu:%zu: %s\n",
        test_name,
        error.span.offset,
        error.span.length,
        error.message
    );
    return false;
  }

  bool passed = true;

  if (program->binding_count != 1) {
    report_failure(test_name, "expected one binding");
    passed = false;
    goto cleanup;
  }

  Binding *binding = &program->bindings[0];

  if (!span_equals(source, binding->name, "p")) {
    report_failure(test_name, "incorrect binding name");
    passed = false;
  }

  if (binding->initializer == NULL
      || binding->initializer->kind != EXPR_PIPELINE) {
    report_failure(test_name, "expected pipeline initializer");
    passed = false;
  }

  if (program->result == NULL
      || program->result->kind != EXPR_PIPELINE) {
    report_failure(test_name, "expected pipeline result");
    passed = false;
  }

cleanup:
  program_destroy(program);

  if (passed) {
    printf("PASS %s\n", test_name);
  }

  return passed;
}

static bool test_program_comments(void) {
  const char *source =
      "// program\n"
      "let // binding name\n"
      "p = uv // input\n"
      ".center(); // binding\n"
      "p. // stage\n"
      "vec4(0.0, // blue\n"
      "1.0) // result";
  Program *program = NULL;
  ParseError error;

  if (!parse_program(source, strlen(source), &program, &error)) {
    report_failure("program comments", error.message);
    return false;
  }

  bool passed = program->binding_count == 1
      && span_equals(source, program->bindings[0].name, "p")
      && program->result->kind == EXPR_PIPELINE
      && span_equals(source, program->result->as.pipeline.stage->as.call.name, "vec4")
      && program->result->as.pipeline.stage->as.call.arguments.count == 2
      && span_equals(source,
          program->result->as.pipeline.stage->as.call.arguments.items[1]->span, "1.0");
  program_destroy(program);
  if (passed) {
    printf("PASS program comments\n");
  } else {
    report_failure("program comments", "incorrect AST or source spans");
  }
  return passed;
}

static bool test_failures(void) {
  const char *test_name = "parse failures";

  static const FailureCase cases[] = {
    {
      .source = "// ignored",
      .message = "expected expression",
      .offset = 10,
      .length = 0,
    },
    {
      .source = "// note\nuv/",
      .message = "invalid token",
      .offset = 10,
      .length = 1,
    },
    {
      .source = "/* block */ uv",
      .message = "invalid token",
      .offset = 0,
      .length = 1,
    },
    {
      .source = "",
      .message = "expected expression",
      .offset = 0,
      .length = 0,
    },
    {
      .source = "uv | center()",
      .message = "invalid token",
      .offset = 3,
      .length = 1,
    },
    {
      .source = "uv |> center()",
      .message = "invalid token",
      .offset = 3,
      .length = 1,
    },
    {
      .source = "uv.",
      .message = "expected function call after '.'",
      .offset = 3,
      .length = 0,
    },
    {
      .source = "uv.center",
      .message = "expected function call after '.'",
      .offset = 9,
      .length = 0,
    },
    {
      .source = "scale(2,5,)",
      .message = "expected expression",
      .offset = 10,
      .length = 1,
    },
    {
      .source = "center(uv",
      .message = "expected ')' after arguments",
      .offset = 9,
      .length = 0,
    },
  };

  size_t case_count = sizeof(cases) / sizeof(cases[0]);
  bool passed = true;

  for (size_t index = 0; index < case_count; index += 1) {
    const FailureCase *test_case = &cases[index];

    Expr *root = NULL;
    ParseError error;

    bool success = parse_source(
        test_case->source,
        strlen(test_case->source),
        &root,
        &error
    );

    if (success) {
      fprintf(
          stderr,
          "FAIL %s: expected failure for \"%s\"\n",
          test_name,
          test_case->source
      );

      expr_destroy(root);
      passed = false;
      continue;
    }

    if (root != NULL) {
      report_failure(
          test_name,
          "parser returned an AST after failure"
      );

      expr_destroy(root);
      passed = false;
    }

    if (error.message == NULL
        || strcmp(error.message, test_case->message) != 0
        || error.span.offset != test_case->offset
        || error.span.length != test_case->length) {
      fprintf(
          stderr,
          "FAIL %s for \"%s\"\n"
          "  expected: %s at %zu:%zu\n"
          "  actual:   %s at %zu:%zu\n",
          test_name,
          test_case->source,
          test_case->message,
          test_case->offset,
          test_case->length,
          error.message == NULL
              ? "(no message)"
              : error.message,
          error.span.offset,
          error.span.length
      );

      passed = false;
    }
  }

  if (passed) {
    printf("PASS %s\n", test_name);
  }

  return passed;
}

static bool test_program_failures(void) {
  const char *test_name = "program parse failures";

  static const FailureCase cases[] = {
    {
      .source = "let = uv; uv",
      .message = "expected binding name after 'let'",
      .offset = 4,
      .length = 1,
    },
    {
      .source = "let p = uv\np",
      .message = "expected ';' after binding",
      .offset = 11,
      .length = 1,
    },
  };

  size_t case_count = sizeof(cases) / sizeof(cases[0]);
  bool passed = true;

  for (size_t index = 0; index < case_count; index += 1) {
    const FailureCase *test_case = &cases[index];
    Program *program = NULL;
    ParseError error;

    bool success = parse_program(
        test_case->source,
        strlen(test_case->source),
        &program,
        &error
    );

    if (success
        || error.message == NULL
        || strcmp(error.message, test_case->message) != 0
        || error.span.offset != test_case->offset
        || error.span.length != test_case->length) {
      fprintf(
          stderr,
          "FAIL %s for \"%s\"\n"
          "  expected: %s at %zu:%zu\n"
          "  actual:   %s at %zu:%zu\n",
          test_name,
          test_case->source,
          test_case->message,
          test_case->offset,
          test_case->length,
          error.message == NULL
              ? "(no message)"
              : error.message,
          error.span.offset,
          error.span.length
      );
      passed = false;
    }

    program_destroy(program);
  }

  if (passed) {
    printf("PASS %s\n", test_name);
  }

  return passed;
}

int main(void) {
  size_t passed = 0;
  size_t test_count = 7;

  if (test_identifier()) {
    passed += 1;
  }

  if (test_call()) {
    passed += 1;
  }

  if (test_pipeline()) {
    passed += 1;
  }

  if (test_program()) {
    passed += 1;
  }

  if (test_program_comments()) {
    passed += 1;
  }

  if (test_failures()) {
    passed += 1;
  }

  if (test_program_failures()) {
    passed += 1;
  }

  printf("%zu/%zu parser tests passed\n", passed, test_count);

  return passed == test_count ? EXIT_SUCCESS : EXIT_FAILURE;
}
