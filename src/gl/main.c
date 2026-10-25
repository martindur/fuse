#define _POSIX_C_SOURCE 200809L

#include "glsl_emit.h"
#include "glsl_stdlib.h"
#include "fuse/help.h"
#include "fuse/lower.h"
#include "fuse/lsp.h"
#include "fuse/parser.h"
#include "fuse/preview.h"
#include "fuse/semantic.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static void usage(FILE *output)
{
    fprintf(
        output,
        "Usage: fusegl [OPTION]... < source.pgl > shader.glsl\n"
        "       fusegl lsp\n"
        "\n"
        "Compile a FuseGL program to a GLSL fragment shader.\n"
        "\n"
        "Commands:\n"
        "  lsp               run the language server over stdin/stdout\n"
        "\n"
        "Options:\n"
        "  --preview-line N  compile the intermediate result at source line N\n"
        "  --builtins         list available functions and globals\n"
        "  -h, --help         show this help\n"
        "\n"
        "Examples:\n"
        "  fusegl < main.pgl > main.frag\n"
        "  fusegl --preview-line 4 < main.pgl > preview.frag\n"
    );
}

static bool parse_line(const char *text, size_t *out_line)
{
    if (text == NULL || text[0] == '\0') {
        return false;
    }

    for (const char *character = text;
         *character != '\0';
         character += 1) {
        if (*character < '0' || *character > '9') {
            return false;
        }
    }

    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);

    if (errno != 0
        || end == text
        || *end != '\0'
        || value == 0
        || value > SIZE_MAX) {
        return false;
    }

    *out_line = (size_t)value;
    return true;
}

int main(int argument_count, char **arguments)
{
    const FuseDialect *dialect = fusegl_dialect();

    if (argument_count > 1 && strcmp(arguments[1], "lsp") == 0) {
        if (argument_count == 3
            && (strcmp(arguments[2], "--help") == 0
                || strcmp(arguments[2], "-h") == 0)) {
            puts("Usage: fusegl lsp\n\nRun the FuseGL language server over JSON-RPC on stdin/stdout.");
            return EXIT_SUCCESS;
        }

        if (argument_count != 2) {
            usage(stderr);
            return EXIT_FAILURE;
        }

        return fuse_lsp_run(dialect, stdin, stdout, stderr);
    }

    size_t preview_line = 0;

    for (int index = 1; index < argument_count; index += 1) {
        if (strcmp(arguments[index], "--help") == 0
            || strcmp(arguments[index], "-h") == 0) {
            usage(stdout);
            return EXIT_SUCCESS;
        }

        if (strcmp(arguments[index], "--builtins") == 0) {
            if (argument_count != 2) {
                usage(stderr);
                return EXIT_FAILURE;
            }

            language_print_builtins(stdout, dialect->environment);
            return EXIT_SUCCESS;
        }

        if (strcmp(arguments[index], "--preview-line") != 0
            || preview_line != 0
            || index + 1 >= argument_count
            || !parse_line(arguments[index + 1], &preview_line)) {
            usage(stderr);
            return EXIT_FAILURE;
        }

        index += 1;
    }

    char *source = NULL;
    size_t capacity = 0;

    /*
     * A NUL byte cannot be part of FuseGL source, so getdelim reads until
     * end-of-file and gives us the complete input as one owned buffer.
     */
    ssize_t length = getdelim(
        &source,
        &capacity,
        '\0',
        stdin
    );

    if (length < 0) {
        if (ferror(stdin)) {
            perror("fusegl: failed to read stdin");
        } else {
            fprintf(stderr, "fusegl: expected an expression on stdin\n");
        }

        free(source);
        return EXIT_FAILURE;
    }

    Program *syntax = NULL;
    Program *preview = NULL;
    Program *lowered = NULL;
    SemanticInfo semantic_info = {0};
    int status = EXIT_FAILURE;
    ParseError parse_error;

    if (!parse_program(
            source,
            (size_t)length,
            &syntax,
            &parse_error
        )) {
        fprintf(
            stderr,
            "fusegl: parse error at %zu:%zu: %s\n",
            source_position_at(
                source,
                (size_t)length,
                parse_error.span.offset
            ).line,
            source_position_at(
                source,
                (size_t)length,
                parse_error.span.offset
            ).column,
            parse_error.message
        );
        goto cleanup;
    }

    const Program *selected = syntax;

    if (preview_line != 0) {
        PreviewError preview_error;

        if (!preview_program_at_line(
                syntax,
                source,
                preview_line,
                &preview,
                &preview_error
            )) {
            fprintf(
                stderr,
                "fusegl: preview error at %zu:%zu: %s\n",
                source_position_at(
                    source,
                    (size_t)length,
                    preview_error.span.offset
                ).line,
                source_position_at(
                    source,
                    (size_t)length,
                    preview_error.span.offset
                ).column,
                preview_error.message
            );
            goto cleanup;
        }

        selected = preview;
    }

    LowerError lower_error;

    if (!lower_program(selected, &lowered, &lower_error)) {
        fprintf(
            stderr,
            "fusegl: lowering error at %zu:%zu: %s\n",
            source_position_at(
                source,
                (size_t)length,
                lower_error.span.offset
            ).line,
            source_position_at(
                source,
                (size_t)length,
                lower_error.span.offset
            ).column,
            lower_error.message
        );
        goto cleanup;
    }

    SemanticError semantic_error;

    if (!semantic_analyze(
            lowered,
            source,
            dialect->environment,
            &semantic_info,
            &semantic_error
        )) {
        fprintf(
            stderr,
            "fusegl: semantic error at %zu:%zu: %s\n",
            source_position_at(
                source,
                (size_t)length,
                semantic_error.span.offset
            ).line,
            source_position_at(
                source,
                (size_t)length,
                semantic_error.span.offset
            ).column,
            semantic_error.message
        );
        goto cleanup;
    }

    if ((preview_line == 0
            && semantic_info.result_type != dialect->required_result_type)
        || (preview_line != 0
            && !value_type_is_previewable(
                semantic_info.result_type
            ))) {
        fprintf(
            stderr,
            "fusegl: semantic error at %zu:%zu: %s\n",
            source_position_at(
                source,
                (size_t)length,
                lowered->result->span.offset
            ).line,
            source_position_at(
                source,
                (size_t)length,
                lowered->result->span.offset
            ).column,
            preview_line == 0
                ? "program result must be vec4"
                : "preview result has unsupported type"
        );
        goto cleanup;
    }

    if (!glsl_emit(
            stdout,
            lowered,
            &semantic_info,
            source
        )) {
        perror("fusegl: failed to write GLSL");
        goto cleanup;
    }

    if (fflush(stdout) == EOF) {
        perror("fusegl: failed to flush stdout");
        goto cleanup;
    }

    status = EXIT_SUCCESS;

cleanup:
    semantic_info_destroy(&semantic_info);
    program_destroy(lowered);
    program_destroy(preview);
    program_destroy(syntax);
    free(source);
    return status;
}
