#include "fuse/lexer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_LENGTH(array) \
  (sizeof(array) / sizeof((array)[0]))

typedef struct {
  TokenKind kind;
  size_t offset;
  size_t length;
} ExpectedToken;

typedef struct {
  const char *name;
  const char *source;
  const ExpectedToken *expected;
  size_t expected_count;
} LexerTestCase;

static const ExpectedToken EMPTY_TOKENS[] = {
  {TOKEN_EOF, 0, 0},
};

static const ExpectedToken IDENTIFIER_TOKENS[] = {
  {TOKEN_IDENTIFIER, 0, 2},
  {TOKEN_EOF, 2, 0},
};

static const ExpectedToken PIPELINE_TOKENS[] = {
  {TOKEN_IDENTIFIER, 0, 2},
  {TOKEN_DOT, 2, 1},
  {TOKEN_IDENTIFIER, 3, 6},
  {TOKEN_LEFT_PAREN, 9, 1},
  {TOKEN_RIGHT_PAREN, 10, 1},
  {TOKEN_EOF, 11, 0},
};

static const ExpectedToken CALL_TOKENS[] = {
  {TOKEN_IDENTIFIER, 0, 5},
  {TOKEN_LEFT_PAREN, 5, 1},
  {TOKEN_NUMBER, 6, 3},
  {TOKEN_COMMA, 9, 1},
  {TOKEN_IDENTIFIER, 11, 4},
  {TOKEN_RIGHT_PAREN, 15, 1},
  {TOKEN_EOF, 16, 0},
};

static const ExpectedToken WHITESPACE_TOKENS[] = {
  {TOKEN_IDENTIFIER, 2, 2},
  {TOKEN_EOF, 6, 0},
};

static const ExpectedToken INVALID_TOKENS[] = {
  {TOKEN_IDENTIFIER, 0, 2},
  {TOKEN_INVALID, 3, 1},
  {TOKEN_IDENTIFIER, 5, 6},
  {TOKEN_LEFT_PAREN, 11, 1},
  {TOKEN_RIGHT_PAREN, 12, 1},
  {TOKEN_EOF, 13, 0},
};

static const ExpectedToken NUMBER_BOUNDARY_TOKENS[] = {
  {TOKEN_NUMBER, 0, 2},
  {TOKEN_NUMBER, 3, 4},
  {TOKEN_NUMBER, 8, 1},
  {TOKEN_DOT, 9, 1},
  {TOKEN_EOF, 10, 0},
};

static const ExpectedToken SIGNED_NUMBER_TOKENS[] = {
  {TOKEN_NUMBER, 0, 5},
  {TOKEN_NUMBER, 6, 3},
  {TOKEN_EOF, 9, 0},
};

static const ExpectedToken BINDING_TOKENS[] = {
  {TOKEN_LET, 0, 3},
  {TOKEN_IDENTIFIER, 4, 1},
  {TOKEN_EQUAL, 6, 1},
  {TOKEN_IDENTIFIER, 8, 2},
  {TOKEN_SEMICOLON, 10, 1},
  {TOKEN_IDENTIFIER, 12, 7},
  {TOKEN_EOF, 19, 0},
};

static const LexerTestCase TEST_CASES[] = {
  {
    .name = "empty input",
    .source = "",
    .expected = EMPTY_TOKENS,
    .expected_count = ARRAY_LENGTH(EMPTY_TOKENS),
  },
  {
    .name = "identifier",
    .source = "uv",
    .expected = IDENTIFIER_TOKENS,
    .expected_count = ARRAY_LENGTH(IDENTIFIER_TOKENS),
  },
  {
    .name = "pipeline",
    .source = "uv.center()",
    .expected = PIPELINE_TOKENS,
    .expected_count = ARRAY_LENGTH(PIPELINE_TOKENS),
  },
  {
    .name = "call with arguments",
    .source = "scale(2.5, time)",
    .expected = CALL_TOKENS,
    .expected_count = ARRAY_LENGTH(CALL_TOKENS),
  },
  {
    .name = "whitespace",
    .source = " \tuv\r\n",
    .expected = WHITESPACE_TOKENS,
    .expected_count = ARRAY_LENGTH(WHITESPACE_TOKENS),
  },
  {
    .name = "invalid token",
    .source = "uv | center()",
    .expected = INVALID_TOKENS,
    .expected_count = ARRAY_LENGTH(INVALID_TOKENS),
  },
  {
    .name = "number boundaries",
    .source = "12 3.14 5.",
    .expected = NUMBER_BOUNDARY_TOKENS,
    .expected_count = ARRAY_LENGTH(NUMBER_BOUNDARY_TOKENS),
  },
  {
    .name = "signed numbers",
    .source = "-0.02 1.5",
    .expected = SIGNED_NUMBER_TOKENS,
    .expected_count = ARRAY_LENGTH(SIGNED_NUMBER_TOKENS),
  },
  {
    .name = "binding",
    .source = "let p = uv; letdown",
    .expected = BINDING_TOKENS,
    .expected_count = ARRAY_LENGTH(BINDING_TOKENS),
  },
};

static bool run_test(const LexerTestCase *test_case) {
  Lexer lexer;
  lexer_init(&lexer, test_case->source, strlen(test_case->source));

  for (size_t index = 0; index < test_case->expected_count; index += 1) {
    Token actual = lexer_next(&lexer);
    ExpectedToken expected = test_case->expected[index];

    if (actual.kind != expected.kind
        || actual.span.offset != expected.offset
        || actual.span.length != expected.length) {
      fprintf(
          stderr,
          "FAIL %s at token %zu\n"
          "  expected: %s at %zu:%zu\n"
          "  actual:   %s at %zu:%zu\n",
          test_case->name,
          index,
          token_kind_name(expected.kind),
          expected.offset,
          expected.length,
          token_kind_name(actual.kind),
          actual.span.offset,
          actual.span.length
      );

      return false;
    }
  }

  printf("PASS %s\n", test_case->name);
  return true;
}

int main(void) {
  size_t passed = 0;
  size_t test_count = ARRAY_LENGTH(TEST_CASES);

  for (size_t index = 0; index < test_count; index += 1) {
    if (run_test(&TEST_CASES[index])) {
      passed += 1;
    }
  }

  printf("%zu/%zu tests passed\n", passed, test_count);

  return passed == test_count ? EXIT_SUCCESS : EXIT_FAILURE;
}
