#define _POSIX_C_SOURCE 200809L

#include "cli/commands.h"
#include "grafic/compile.h"
#include "glsl_emit.h"
#include "glsl_stdlib.h"
#include "grafic/help.h"

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
        "Usage: grafic gl [OPTION]... < source > shader.glsl\n"
        "\n"
        "Compile a Grafic program to a GLSL fragment or vertex shader.\n"
        "\n"
        "Options:\n"
        "  --stage STAGE      fragment (default) or vertex\n"
        "  --preview-line N   compile the intermediate result at source line N\n"
        "  --builtins         list available functions and globals\n"
        "  -h, --help         show this help\n"
        "\n"
        "Examples:\n"
        "  grafic gl < main.gfrag > main.frag\n"
        "  grafic gl --stage vertex < waves.gvert > waves.vert\n"
        "  grafic gl --preview-line 4 < main.gfrag > preview.frag\n"
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

int grafic_gl_main(int argument_count, char **arguments)
{
    ShaderStage stage = SHADER_STAGE_FRAGMENT;
    bool stage_set = false;
    bool builtins = false;

    size_t preview_line = 0;

    for (int index = 1; index < argument_count; index += 1) {
        if (strcmp(arguments[index], "--help") == 0
            || strcmp(arguments[index], "-h") == 0) {
            usage(stdout);
            return EXIT_SUCCESS;
        }

        if (strcmp(arguments[index], "--builtins") == 0 && !builtins) {
            builtins = true;
            continue;
        }
        if (strcmp(arguments[index], "--stage") == 0 && !stage_set && index + 1 < argument_count) {
            const char *name = arguments[++index];
            if (strcmp(name, "vertex") == 0) stage = SHADER_STAGE_VERTEX;
            else if (strcmp(name, "fragment") != 0) { usage(stderr); return EXIT_FAILURE; }
            stage_set = true;
            continue;
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

    const GraficDialect *dialect = gl_dialect(stage);
    if (builtins) {
        if (preview_line != 0) { usage(stderr); return EXIT_FAILURE; }
        language_print_builtins(stdout, dialect->environment);
        return EXIT_SUCCESS;
    }

    char *source = NULL;
    size_t capacity = 0;

    /*
     * A NUL byte cannot be part of Grafic shader source, so getdelim reads until
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
            perror("grafic gl: failed to read stdin");
        } else {
            fprintf(stderr, "grafic gl: expected an expression on stdin\n");
        }

        free(source);
        return EXIT_FAILURE;
    }

    PreparedProgram prepared = {0};
    int status = EXIT_FAILURE;
    CompileError compile_error;
    if (!prepare_program(source, (size_t)length, dialect, preview_line, &prepared, &compile_error)) {
        SourcePosition position = source_position_at(source, (size_t)length, compile_error.span.offset);
        fprintf(stderr, "grafic gl: %s error at %zu:%zu: %s\n", compile_error.kind,
            position.line, position.column, compile_error.message);
        goto cleanup;
    }

    if (!glsl_emit(
            stdout, stage,
            &prepared,
            source
        )) {
        perror("grafic gl: failed to write GLSL");
        goto cleanup;
    }

    if (fflush(stdout) == EOF) {
        perror("grafic gl: failed to flush stdout");
        goto cleanup;
    }

    status = EXIT_SUCCESS;

cleanup:
    prepared_program_destroy(&prepared);
    free(source);
    return status;
}
