#define _POSIX_C_SOURCE 200809L

#include "fuse/ast_dump.h"
#include "fuse/parser.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char EXPECTED_PIPELINE_DUMP[] =
    "(pipeline\n"
    "  (pipeline\n"
    "    (identifier \"uv\")\n"
    "    (call \"center\"))\n"
    "  (call \"rotate\"\n"
    "    (identifier \"time\")))\n";

static bool dump_to_string(
    const Expr *expression,
    const char *source,
    char **out_text,
    size_t *out_length
)
{
    FILE *stream = open_memstream(out_text, out_length);

    if (stream == NULL) {
        perror("ast_dump_test: open_memstream");
        return false;
    }

    bool dumped = ast_dump(stream, expression, source);

    /*
     * fclose finalizes the memory stream. Only after this call are
     * `*out_text` and `*out_length` guaranteed to contain the final
     * buffer and byte count.
     */
    bool closed = fclose(stream) != EOF;

    return dumped && closed;
}

static bool test_pipeline_dump(void)
{
    const char *source =
        "uv.center().rotate(time)";

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
            "FAIL AST dump: parse error at %zu:%zu: %s\n",
            error.span.offset,
            error.span.length,
            error.message
        );

        return false;
    }

    char *actual = NULL;
    size_t actual_length = 0;

    if (!dump_to_string(
            root,
            source,
            &actual,
            &actual_length
        )) {
        fprintf(
            stderr,
            "FAIL AST dump: could not capture output\n"
        );

        expr_destroy(root);
        free(actual);
        return false;
    }

    bool matches = (sizeof(EXPECTED_PIPELINE_DUMP) - 1) == actual_length
      && memcmp(actual, EXPECTED_PIPELINE_DUMP, actual_length) == 0;

    if (!matches) {
        fprintf(
            stderr,
            "FAIL AST dump snapshot\n"
            "expected (%zu bytes):\n%s"
            "actual (%zu bytes):\n%s",
            sizeof(EXPECTED_PIPELINE_DUMP) - 1,
            EXPECTED_PIPELINE_DUMP,
            actual_length,
            actual == NULL ? "(null)\n" : actual
        );
    }

    expr_destroy(root);
    free(actual);

    if (matches) {
        printf("PASS AST dump snapshot\n");
    }

    return matches;
}

int main(void)
{
    bool passed = test_pipeline_dump();

    printf(
        "%zu/1 AST dump tests passed\n",
        passed ? (size_t)1 : (size_t)0
    );

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
