#define _POSIX_C_SOURCE 200809L

#include "cli/commands.h"
#include "fuse/compile.h"
#include "glsl_emit.h"
#include "glsl_stdlib.h"
#include "fuse/help.h"

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
        "Usage: fuse gl [OPTION]... < source.pgl > shader.glsl\n"
        "\n"
        "Compile a FuseGL program to a GLSL fragment shader.\n"
        "\n"
        "Options:\n"
        "  --preview-line N  compile the intermediate result at source line N\n"
        "  --builtins         list available functions and globals\n"
        "  -h, --help         show this help\n"
        "\n"
        "Examples:\n"
        "  fuse gl < main.pgl > main.frag\n"
        "  fuse gl --preview-line 4 < main.pgl > preview.frag\n"
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

int fuse_gl_main(int argument_count, char **arguments)
{
    const FuseDialect *dialect = fusegl_dialect();

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
            perror("fuse gl: failed to read stdin");
        } else {
            fprintf(stderr, "fuse gl: expected an expression on stdin\n");
        }

        free(source);
        return EXIT_FAILURE;
    }

    PreparedProgram prepared = {0};
    int status = EXIT_FAILURE;
    CompileError compile_error;
    if (!prepare_program(source, (size_t)length, dialect, preview_line, &prepared, &compile_error)) {
        SourcePosition position = source_position_at(source, (size_t)length, compile_error.span.offset);
        fprintf(stderr, "fuse gl: %s error at %zu:%zu: %s\n", compile_error.kind,
            position.line, position.column, compile_error.message);
        goto cleanup;
    }

    if (!glsl_emit(
            stdout,
            prepared.program,
            &prepared.semantic_info,
            source
        )) {
        perror("fuse gl: failed to write GLSL");
        goto cleanup;
    }

    if (fflush(stdout) == EOF) {
        perror("fuse gl: failed to flush stdout");
        goto cleanup;
    }

    status = EXIT_SUCCESS;

cleanup:
    prepared_program_destroy(&prepared);
    free(source);
    return status;
}
