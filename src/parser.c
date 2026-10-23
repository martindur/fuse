#include "fuse/parser.h"

#include "fuse/lexer.h"

typedef struct {
  Lexer lexer;
  Token current;
  Token previous;
  bool has_error;
  ParseError error;
} Parser;

static Expr *parse_expression(Parser *parser);

static void parser_fail(
    Parser *parser,
    SourceSpan span,
    const char *message
) {
  if (parser->has_error) {
    return;
  }

  parser->has_error = true;
  parser->error.span = span;
  parser->error.message = message;
}

static void parser_advance(Parser *parser) {
  parser->previous = parser->current;
  parser->current = lexer_next(&parser->lexer);
}

static bool parser_check(const Parser *parser, TokenKind kind) {
  return parser->current.kind == kind;
}

static bool parser_match(Parser *parser, TokenKind kind) {
  if (!parser_check(parser, kind)) {
    return false;
  }

  parser_advance(parser);
  return true;
}

static bool parser_consume(
    Parser *parser,
    TokenKind kind,
    const char *message
) {
  if (parser_match(parser, kind)) {
    return true;
  }

  parser_fail(parser, parser->current.span, message);
  return false;
}

static SourceSpan span_covering(
    SourceSpan first,
    SourceSpan last
) {
  size_t end = last.offset + last.length;

  return (SourceSpan){
    .offset = first.offset,
    .length = end - first.offset,
  };
}

static Expr *parse_call_after_name(
    Parser *parser,
    Token name
) {
  parser_advance(parser);

  Expr *call = expr_create_call(name.span, name.span);

  if (call == NULL) {
    parser_fail(parser, name.span, "out of memory");
    return NULL;
  }

  if (!parser_check(parser, TOKEN_RIGHT_PAREN)) {
    for (;;) {
      Expr *argument = parse_expression(parser);

      if (argument == NULL) {
        expr_destroy(call);
        return NULL;
      }

      if (!expr_call_add_argument(call, argument)) {
        expr_destroy(argument);
        expr_destroy(call);
        parser_fail(parser, name.span, "out of memory");
        return NULL;
      }

      if (!parser_match(parser, TOKEN_COMMA)) {
        break;
      }
    }
  }

  if (!parser_consume(
        parser,
        TOKEN_RIGHT_PAREN,
        "expected ')' after arguments"
        )) {
    expr_destroy(call);
    return NULL;
  }

  call->span = span_covering(name.span, parser->previous.span);
  return call;
}

static Expr *parse_primary(Parser *parser) {
  if (parser_check(parser, TOKEN_IDENTIFIER)) {
    Token identifier = parser->current;
    parser_advance(parser);

    if (parser_check(parser, TOKEN_LEFT_PAREN)) {
      return parse_call_after_name(parser, identifier);
    }

    Expr *expression = expr_create_identifier(identifier.span);

    if (expression == NULL) {
      parser_fail(parser, identifier.span, "out of memory");
    }

    return expression;
  }

  if (parser_check(parser, TOKEN_NUMBER)) {
    Token number = parser->current;
    parser_advance(parser);

    Expr *expression = expr_create_number(number.span);

    if (expression == NULL) {
      parser_fail(parser, number.span, "out of memory");
    }

    return expression;
  }

  if (parser_match(parser, TOKEN_LEFT_PAREN)) {
    Expr *expression = parse_expression(parser);

    if (expression == NULL) {
      return NULL;
    }

    if (!parser_consume(
          parser,
          TOKEN_RIGHT_PAREN,
          "expected ')' after expression"
      )) {
      expr_destroy(expression);
      return NULL;
    }

    return expression;
  }

  if (parser_check(parser, TOKEN_INVALID)) {
    parser_fail(
        parser,
        parser->current.span,
        "invalid token"
        );
  } else {
    parser_fail(
        parser,
        parser->current.span,
        "expected expression"
        );
  }

  return NULL;
}

static Expr *parse_pipeline(Parser *parser) {
  Expr *input = parse_primary(parser);

  if (input == NULL) {
    return NULL;
  }

  while (parser_match(parser, TOKEN_DOT)) {
    if (!parser_check(parser, TOKEN_IDENTIFIER)) {
      parser_fail(
          parser,
          parser->current.span,
          "expected function call after '.'"
      );
      expr_destroy(input);
      return NULL;
    }

    Token name = parser->current;
    parser_advance(parser);

    if (!parser_check(parser, TOKEN_LEFT_PAREN)) {
      parser_fail(
          parser,
          parser->current.span,
          "expected function call after '.'"
      );
      expr_destroy(input);
      return NULL;
    }

    Expr *stage = parse_call_after_name(parser, name);

    if (stage == NULL) {
      expr_destroy(input);
      return NULL;
    }

    SourceSpan span = span_covering(input->span, stage->span);

    Expr *pipeline = expr_create_pipeline(
        span,
        input,
        stage
    );

    if (pipeline == NULL) {
      expr_destroy(input);
      expr_destroy(stage);
      parser_fail(parser, span, "out of memory");
      return NULL;
    }

    input = pipeline;
  }

  return input;
}

static Expr *parse_expression(Parser *parser) {
  return parse_pipeline(parser);
}

bool parse_source(
    const char *source,
    size_t length,
    Expr **out_expression,
    ParseError *out_error
) {
  if (out_expression == NULL || out_error == NULL) {
    return false;
  }

  *out_expression = NULL;
  *out_error = (ParseError){0};

  if (source == NULL) {
    out_error->message = "source is NULL";
    return false;
  }

  Parser parser = {0};
  lexer_init(&parser.lexer, source, length);
  parser_advance(&parser);

  Expr *expression = parse_expression(&parser);

  if (expression != NULL && !parser.has_error) {
    if (parser_check(&parser, TOKEN_INVALID)) {
      parser_fail(
          &parser,
          parser.current.span,
          "invalid token"
      );
    } else if (!parser_check(&parser, TOKEN_EOF)) {
      parser_fail(
          &parser,
          parser.current.span,
          "unexpected token after expression"
      );
    }
  }

  if (parser.has_error || expression == NULL) {
    expr_destroy(expression);
    *out_error = parser.error;
    return false;
  }

  *out_expression = expression;
  return true;
}

static bool parse_binding(
    Parser *parser,
    Program *program
) {
  parser_advance(parser);

  if (!parser_consume(
        parser,
        TOKEN_IDENTIFIER,
        "expected binding name after 'let'"
      )) {
    return false;
  }

  Token name = parser->previous;

  if (!parser_consume(
        parser,
        TOKEN_EQUAL,
        "expected '=' after binding name"
      )) {
    return false;
  }

  Expr *initializer = parse_expression(parser);

  if (initializer == NULL) {
    return false;
  }

  if (!parser_consume(
        parser,
        TOKEN_SEMICOLON,
        "expected ';' after binding"
      )) {
    expr_destroy(initializer);
    return false;
  }

  if (!program_add_binding(
        program,
        name.span,
        initializer
      )) {
    expr_destroy(initializer);
    parser_fail(parser, name.span, "out of memory");
    return false;
  }

  return true;
}

bool parse_program(
    const char *source,
    size_t length,
    Program **out_program,
    ParseError *out_error
) {
  if (out_program == NULL || out_error == NULL) {
    return false;
  }

  *out_program = NULL;
  *out_error = (ParseError){0};

  if (source == NULL) {
    out_error->message = "source is NULL";
    return false;
  }

  Parser parser = {0};
  lexer_init(&parser.lexer, source, length);
  parser_advance(&parser);

  Program *program = program_create();

  if (program == NULL) {
    out_error->message = "out of memory";
    return false;
  }

  while (parser_check(&parser, TOKEN_LET)) {
    if (!parse_binding(&parser, program)) {
      program_destroy(program);
      *out_error = parser.error;
      return false;
    }
  }

  Expr *result = parse_expression(&parser);

  if (result != NULL && !parser.has_error) {
    if (parser_check(&parser, TOKEN_INVALID)) {
      parser_fail(
          &parser,
          parser.current.span,
          "invalid token"
      );
    } else if (!parser_check(&parser, TOKEN_EOF)) {
      parser_fail(
          &parser,
          parser.current.span,
          "unexpected token after expression"
      );
    }
  }

  if (parser.has_error || result == NULL) {
    expr_destroy(result);
    program_destroy(program);
    *out_error = parser.error;
    return false;
  }

  if (!program_set_result(program, result)) {
    expr_destroy(result);
    program_destroy(program);
    out_error->message = "out of memory";
    return false;
  }

  *out_program = program;
  return true;
}
