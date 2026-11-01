#define _POSIX_C_SOURCE 200809L

#include "cli/commands.h"
#include "fuse/compile.h"
#include "builtins.h"
#include "fuse/help.h"
#include "render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *output, const char *program)
{
    fprintf(
        output,
        "Usage: %s [OPTION]... < source.pmesh > mesh.obj\n"
        "\n"
        "Evaluate a FuseMesh program and write an OBJ mesh.\n"
        "\n"
        "Options:\n"
        "  --preview-line N  compile the intermediate result at source line N\n"
        "  --builtins         list available functions\n"
        "  -h, --help         show this help\n"
        "\n"
        "Examples:\n"
        "  fuse mesh < mesh.pmesh > mesh.obj\n"
        "  fuse mesh --preview-line 4 < mesh.pmesh > preview.obj\n",
        program
    );
}

static bool parse_line(const char *text, size_t *out_line)
{
    if (text == NULL || text[0] == '\0') {
        return false;
    }

    size_t value = 0;

    for (const char *character = text;
         *character != '\0';
         character += 1) {
        if (*character < '0' || *character > '9') {
            return false;
        }

        size_t digit = (size_t)(*character - '0');

        if (value > (SIZE_MAX - digit) / 10) {
            return false;
        }

        value = value * 10 + digit;
    }

    if (value == 0) {
        return false;
    }

    *out_line = value;
    return true;
}

static void print_error(
    const char *program,
    const char *kind,
    const char *source,
    size_t source_length,
    SourceSpan span,
    const char *message
)
{
    SourcePosition position = source_position_at(
        source,
        source_length,
        span.offset
    );
    fprintf(
        stderr,
        "%s: %s error at %zu:%zu: %s\n",
        program,
        kind,
        position.line,
        position.column,
        message
    );
}

int fuse_mesh_main(int argument_count, char **arguments)
{
    const FuseDialect *dialect = fusemesh_dialect();

    size_t preview_line = 0;

    for (int index = 1; index < argument_count; index += 1) {
        const char *option = arguments[index];

        if (strcmp(option, "--help") == 0
            || strcmp(option, "-h") == 0) {
            usage(stdout, "fuse mesh");
            return EXIT_SUCCESS;
        }

        if (strcmp(option, "--builtins") == 0) {
            if (argument_count != 2) {
                usage(stderr, "fuse mesh");
                return EXIT_FAILURE;
            }

            language_print_builtins(
                stdout,
                dialect->environment
            );
            return EXIT_SUCCESS;
        }

        if (strcmp(option, "--preview-line") != 0
            || index + 1 >= argument_count
            || !parse_line(arguments[++index], &preview_line)) {
            usage(stderr, "fuse mesh");
            return EXIT_FAILURE;
        }
    }

    char *source = NULL;
    size_t capacity = 0;
    ssize_t length = getdelim(&source, &capacity, '\0', stdin);

    if (length < 0) {
        fprintf(stderr, "fuse mesh: expected a program on stdin\n");
        free(source);
        return EXIT_FAILURE;
    }

    PreparedProgram prepared = {0};
    FuseMesh mesh = {0};
    int status = EXIT_FAILURE;
    CompileError compile_error;
    if (!prepare_program(source, (size_t)length, dialect, preview_line, &prepared, &compile_error)) {
        SourcePosition position = source_position_at(source, (size_t)length, compile_error.span.offset);
        fprintf(stderr, "fuse mesh: %s error at %zu:%zu: %s\n", compile_error.kind,
            position.line, position.column, compile_error.message);
        goto cleanup;
    }

    FuseMeshRenderError render_error;

    if (!fusemesh_render(
            prepared.program,
            source,
            &mesh,
            &render_error
        )) {
        print_error(
            "fuse mesh",
            "render",
            source,
            (size_t)length,
            render_error.span,
            render_error.message
        );
        goto cleanup;
    }

    if (!fusemesh_mesh_write_obj(stdout, &mesh)) {
        perror("fuse mesh: failed to write OBJ");
        goto cleanup;
    }

    status = EXIT_SUCCESS;

cleanup:
    fusemesh_mesh_destroy(&mesh);
    prepared_program_destroy(&prepared);
    free(source);
    return status;
}
