#define _POSIX_C_SOURCE 200809L
#include "view/assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool write_source(const char *path, const char *text, size_t length)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }
    bool written = fwrite(text, 1, length, file) == length;
    return fclose(file) == 0 && written;
}

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL viewer source lifecycle at line %d\n", __LINE__); \
    passed = false; goto cleanup; } } while (0)

int main(void)
{
    char directory[] = "/tmp/fuse-view-assets-XXXXXX";
    if (mkdtemp(directory) == NULL) {
        return EXIT_FAILURE;
    }
    char path[256];
    char replacement[256];
    snprintf(path, sizeof(path), "%s/shader.pgl", directory);
    snprintf(replacement, sizeof(replacement), "%s/replacement.pgl", directory);
    const char *valid = "vec4(uv, 0.0, 1.0)";
    const char *changed = "vec4(uv, 1.0, 1.0)";
    const char *invalid = "uv.missing()";
    ViewSource source = {0};
    char error[256];
    CompileError compile_error;
    char *glsl = NULL;
    bool passed = true;
    CHECK(write_source(path, valid, strlen(valid)));
    CHECK(view_source_poll(&source, path, error, sizeof(error)) == VIEW_SOURCE_CHANGED);
    CHECK(view_shader_compile(&source, 0, &glsl, &compile_error));
    CHECK(strstr(glsl, "#version 330 core") != NULL);
    free(glsl);
    glsl = NULL;
    CHECK(view_source_poll(&source, path, error, sizeof(error)) == VIEW_SOURCE_UNCHANGED);

    CHECK(write_source(replacement, changed, strlen(changed)));
    CHECK(rename(replacement, path) == 0);
    CHECK(view_source_poll(&source, path, error, sizeof(error)) == VIEW_SOURCE_CHANGED);
    CHECK(strcmp(source.text, changed) == 0);
    CHECK(write_source(path, invalid, strlen(invalid)));
    CHECK(view_source_poll(&source, path, error, sizeof(error)) == VIEW_SOURCE_CHANGED);
    CHECK(!view_shader_compile(&source, 0, &glsl, &compile_error));
    CHECK(glsl == NULL && strcmp(compile_error.kind, "semantic") == 0);
    CHECK(view_source_poll(&source, path, error, sizeof(error)) == VIEW_SOURCE_UNCHANGED);

    CHECK(unlink(path) == 0);
    CHECK(view_source_poll(&source, path, error, sizeof(error)) == VIEW_SOURCE_ERROR);
    CHECK(!source.available);
    CHECK(!view_shader_compile(&source, 0, &glsl, &compile_error));
    CHECK(write_source(path, invalid, strlen(invalid)));
    CHECK(view_source_poll(&source, path, error, sizeof(error)) == VIEW_SOURCE_CHANGED);
    CHECK(write_source(path, valid, strlen(valid)));
    CHECK(view_source_poll(&source, path, error, sizeof(error)) == VIEW_SOURCE_CHANGED);
    CHECK(view_shader_compile(&source, 0, &glsl, &compile_error));
    free(glsl);
    glsl = NULL;

    const char nul[] = {'u', 'v', '\0'};
    CHECK(write_source(path, nul, sizeof(nul)));
    CHECK(view_source_poll(&source, path, error, sizeof(error)) == VIEW_SOURCE_ERROR);
    CHECK(strstr(error, "NUL") != NULL);
    CHECK(write_source(path, "", 0));
    CHECK(view_source_poll(&source, path, error, sizeof(error)) == VIEW_SOURCE_CHANGED);
    CHECK(!view_shader_compile(&source, 0, &glsl, &compile_error));
    CHECK(strcmp(compile_error.kind, "parse") == 0);
    CHECK(view_source_poll(&source, directory, error, sizeof(error)) == VIEW_SOURCE_ERROR);

cleanup:
    free(glsl);
    view_source_destroy(&source);
    unlink(path);
    unlink(replacement);
    rmdir(directory);
    printf("%s viewer source polling, atomic saves, deletion recovery, and compilation\n", passed ? "PASS" : "FAIL");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
