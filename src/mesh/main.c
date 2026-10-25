#define _POSIX_C_SOURCE 200809L

#include "builtins.h"
#include "fuse/help.h"
#include "fuse/lsp.h"
#include "render.h"

#include "fuse/lower.h"
#include "fuse/parser.h"
#include "fuse/preview.h"
#include "fuse/semantic.h"

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
        "Commands:\n"
        "  lsp               run the language server over stdin/stdout\n"
        "\n"
        "Options:\n"
        "  --preview-line N  compile the intermediate result at source line N\n"
        "  --builtins         list available functions\n"
        "  -h, --help         show this help\n"
        "\n"
        "Examples:\n"
        "  fusemesh < mesh.pmesh > mesh.obj\n"
        "  fusemesh --preview-line 4 < mesh.pmesh > preview.obj\n",
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

int main(int argument_count, char **arguments)
{
    const FuseDialect *dialect = fusemesh_dialect();

    if (argument_count > 1 && strcmp(arguments[1], "lsp") == 0) {
        if (argument_count == 3
            && (strcmp(arguments[2], "--help") == 0
                || strcmp(arguments[2], "-h") == 0)) {
            puts("Usage: fusemesh lsp\n\nRun the language server over JSON-RPC on stdin/stdout.");
            return EXIT_SUCCESS;
        }

        if (argument_count != 2) {
            usage(stderr, arguments[0]);
            return EXIT_FAILURE;
        }

        return fuse_lsp_run(dialect, stdin, stdout, stderr);
    }

    size_t preview_line = 0;

    for (int index = 1; index < argument_count; index += 1) {
        const char *option = arguments[index];

        if (strcmp(option, "--help") == 0
            || strcmp(option, "-h") == 0) {
            usage(stdout, arguments[0]);
            return EXIT_SUCCESS;
        }

        if (strcmp(option, "--builtins") == 0) {
            if (argument_count != 2) {
                usage(stderr, arguments[0]);
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
            usage(stderr, arguments[0]);
            return EXIT_FAILURE;
        }
    }

    char *source = NULL;
    size_t capacity = 0;
    ssize_t length = getdelim(&source, &capacity, '\0', stdin);

    if (length < 0) {
        fprintf(stderr, "fusemesh: expected a program on stdin\n");
        free(source);
        return EXIT_FAILURE;
    }

    Program *syntax = NULL;
    Program *preview = NULL;
    Program *lowered = NULL;
    SemanticInfo semantic_info = {0};
    FuseMesh mesh = {0};
    int status = EXIT_FAILURE;
    ParseError parse_error;

    if (!parse_program(
            source,
            (size_t)length,
            &syntax,
            &parse_error
        )) {
        print_error(
            "fusemesh",
            "parse",
            source,
            (size_t)length,
            parse_error.span,
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
            print_error(
                "fusemesh",
                "preview",
                source,
                (size_t)length,
                preview_error.span,
                preview_error.message
            );
            goto cleanup;
        }

        selected = preview;
    }

    LowerError lower_error;

    if (!lower_program(selected, &lowered, &lower_error)) {
        print_error(
            "fusemesh",
            "lowering",
            source,
            (size_t)length,
            lower_error.span,
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
        print_error(
            "fusemesh",
            "semantic",
            source,
            (size_t)length,
            semantic_error.span,
            semantic_error.message
        );
        goto cleanup;
    }

    if (semantic_info.result_type != dialect->required_result_type) {
        print_error(
            "fusemesh",
            "semantic",
            source,
            (size_t)length,
            lowered->result->span,
            "program result must be a mesh"
        );
        goto cleanup;
    }

    FuseMeshRenderError render_error;

    if (!fusemesh_render(
            lowered,
            source,
            &mesh,
            &render_error
        )) {
        print_error(
            "fusemesh",
            "render",
            source,
            (size_t)length,
            render_error.span,
            render_error.message
        );
        goto cleanup;
    }

    if (!fusemesh_mesh_write_obj(stdout, &mesh)) {
        perror("fusemesh: failed to write OBJ");
        goto cleanup;
    }

    status = EXIT_SUCCESS;

cleanup:
    fusemesh_mesh_destroy(&mesh);
    semantic_info_destroy(&semantic_info);
    program_destroy(lowered);
    program_destroy(preview);
    program_destroy(syntax);
    free(source);
    return status;
}
