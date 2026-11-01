#define _POSIX_C_SOURCE 200809L

#include "cli/commands.h"
#include "builtins.h"
#include "fuse/help.h"
#include "png_write.h"
#include "render.h"

#include "fuse/lower.h"
#include "fuse/parser.h"
#include "fuse/preview.h"
#include "fuse/semantic.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static void usage(FILE *output, const char *program)
{
    fprintf(
        output,
        "Usage: %s [OPTION]... < source.ptex > texture.png\n"
        "\n"
        "Evaluate a FuseTex program and write a PNG image.\n"
        "\n"
        "Options:\n"
        "  --width N          output width in pixels (default: 256)\n"
        "  --height N         output height in pixels (default: 256)\n"
        "  --seed N           seed used by noise (default: 0)\n"
        "  --preview-line N   render the intermediate result at source line N\n"
        "  --builtins         list available functions and globals\n"
        "  -h, --help         show this help\n"
        "\n"
        "Examples:\n"
        "  fuse tex < texture.ptex > texture.png\n"
        "  fuse tex --width 512 --height 256 < texture.ptex > texture.png\n"
        "  fuse tex --preview-line 3 < texture.ptex > preview.png\n",
        program
    );
}

static bool parse_unsigned(
    const char *text,
    size_t *out_value
)
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

    *out_value = (size_t)value;
    return true;
}

static bool parse_seed(
    const char *text,
    uint64_t *out_seed
)
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

    if (errno != 0 || end == text || *end != '\0') {
        return false;
    }

    *out_seed = (uint64_t)value;
    return true;
}

int fuse_tex_main(int argument_count, char **arguments)
{
    const FuseDialect *dialect = fusetex_dialect();

    RenderConfig config = {
        .width = 256,
        .height = 256,
        .seed = 0,
    };
    size_t preview_line = 0;

    for (int index = 1; index < argument_count; index += 1) {
        const char *option = arguments[index];

        if (strcmp(option, "--help") == 0
            || strcmp(option, "-h") == 0) {
            usage(stdout, "fuse tex");
            return EXIT_SUCCESS;
        }

        if (strcmp(option, "--builtins") == 0) {
            if (argument_count != 2) {
                usage(stderr, "fuse tex");
                return EXIT_FAILURE;
            }

            language_print_builtins(
                stdout,
                dialect->environment
            );
            return EXIT_SUCCESS;
        }

        if (strcmp(option, "--width") == 0
            || strcmp(option, "--height") == 0
            || strcmp(option, "--seed") == 0
            || strcmp(option, "--preview-line") == 0) {
            if (index + 1 >= argument_count) {
                usage(stderr, "fuse tex");
                return EXIT_FAILURE;
            }

            const char *value = arguments[++index];

            if (strcmp(option, "--seed") == 0) {
                if (!parse_seed(value, &config.seed)) {
                    fprintf(stderr, "fuse tex: invalid seed: %s\n", value);
                    return EXIT_FAILURE;
                }
            } else {
                size_t *target = NULL;

                if (strcmp(option, "--width") == 0) {
                    target = &config.width;
                } else if (strcmp(option, "--height") == 0) {
                    target = &config.height;
                } else if (preview_line == 0) {
                    target = &preview_line;
                }

                if (target == NULL || !parse_unsigned(value, target)) {
                    fprintf(
                        stderr,
                        "fuse tex: invalid %s: %s\n",
                        strcmp(option, "--preview-line") == 0
                            ? "preview line"
                            : "dimension",
                        value
                    );
                    return EXIT_FAILURE;
                }
            }
        } else {
            usage(stderr, "fuse tex");
            return EXIT_FAILURE;
        }
    }

    char *source = NULL;
    size_t capacity = 0;
    ssize_t length = getdelim(&source, &capacity, '\0', stdin);

    if (length < 0) {
        fprintf(stderr, "fuse tex: expected a program on stdin\n");
        free(source);
        return EXIT_FAILURE;
    }

    Program *syntax = NULL;
    Program *preview = NULL;
    Program *lowered = NULL;
    SemanticInfo semantic_info = {0};
    Image image = {0};
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
            "fuse tex: parse error at %zu:%zu: %s\n",
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
                "fuse tex: preview error at %zu:%zu: %s\n",
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
            "fuse tex: lowering error at %zu:%zu: %s\n",
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
            "fuse tex: semantic error at %zu:%zu: %s\n",
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
            "fuse tex: semantic error at %zu:%zu: %s\n",
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

    RenderError render_error;

    if (!fusetex_render(
            lowered,
            source,
            &config,
            &image,
            &render_error
        )) {
        fprintf(
            stderr,
            "fuse tex: render error at %zu:%zu: %s\n",
            source_position_at(
                source,
                (size_t)length,
                render_error.span.offset
            ).line,
            source_position_at(
                source,
                (size_t)length,
                render_error.span.offset
            ).column,
            render_error.message
        );
        goto cleanup;
    }

    if (!png_write(stdout, &image) || fflush(stdout) == EOF) {
        perror("fuse tex: failed to write PNG");
        goto cleanup;
    }

    status = EXIT_SUCCESS;

cleanup:
    image_destroy(&image);
    semantic_info_destroy(&semantic_info);
    program_destroy(lowered);
    program_destroy(preview);
    program_destroy(syntax);
    free(source);
    return status;
}
