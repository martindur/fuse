#include "commands.h"
#include "grafic/lsp.h"

#ifdef GRAFIC_CAP_GL
#include "gl/glsl_stdlib.h"
#endif
#ifdef GRAFIC_CAP_TEX
#include "tex/builtins.h"
#endif
#ifdef GRAFIC_CAP_MESH
#include "mesh/builtins.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *description;
    int (*run)(int argc, char **argv);
    const GraficDialect *(*dialect)(void);
} Command;

static const Command COMMANDS[] = {
#ifdef GRAFIC_CAP_GL
    {"gl", "compile a GLSL fragment shader", grafic_gl_main, graficgl_dialect},
#endif
#ifdef GRAFIC_CAP_TEX
    {"tex", "generate a PNG texture", grafic_tex_main, grafictex_dialect},
#endif
#ifdef GRAFIC_CAP_MESH
    {"mesh", "generate an OBJ mesh", grafic_mesh_main, graficmesh_dialect},
#endif
};

#define COMMAND_COUNT (sizeof(COMMANDS) / sizeof(COMMANDS[0]))

static void usage(FILE *output)
{
    fputs("Usage: grafic COMMAND [OPTION]...\n\nCommands:\n", output);
    for (size_t index = 0; index < COMMAND_COUNT; index += 1) {
        fprintf(output, "  %-6s %s\n", COMMANDS[index].name, COMMANDS[index].description);
    }
#ifdef GRAFIC_CAP_VIEW
    fputs("  view   preview Grafic shader, texture, and mesh source files\n", output);
#endif
    fputs("  lsp    run the language server\n\nUse grafic COMMAND --help for command options.\n", output);
}

int main(int argc, char **argv)
{
    if (argc == 1 || (argc == 2
            && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0))) {
        usage(stdout);
        return EXIT_SUCCESS;
    }
    for (size_t index = 0; index < COMMAND_COUNT; index += 1) {
        if (strcmp(argv[1], COMMANDS[index].name) == 0) {
            return COMMANDS[index].run(argc - 1, argv + 1);
        }
    }
#ifdef GRAFIC_CAP_VIEW
    if (strcmp(argv[1], "view") == 0) {
        return grafic_view_main(argc - 1, argv + 1);
    }
#endif
    if (strcmp(argv[1], "lsp") == 0) {
        if (argc == 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
            puts("Usage: grafic lsp\n\nServe Grafic documents over stdin/stdout.\n"
                 "Only capabilities included in this build are available.");
            return EXIT_SUCCESS;
        }
        if (argc == 2) {
            const GraficDialect *dialects[COMMAND_COUNT];
            for (size_t index = 0; index < COMMAND_COUNT; index += 1) {
                dialects[index] = COMMANDS[index].dialect();
            }
            return grafic_lsp_run(dialects, COMMAND_COUNT, stdin, stdout, stderr);
        }
    }
    fprintf(stderr, "grafic: unknown command or invalid arguments: %s\n", argv[1]);
    usage(stderr);
    return EXIT_FAILURE;
}
