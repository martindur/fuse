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
    char directory[] = "/tmp/grafic-view-assets-XXXXXX";
    if (mkdtemp(directory) == NULL) {
        return EXIT_FAILURE;
    }
    char path[256];
    char replacement[256];
    snprintf(path, sizeof(path), "%s/shader.gfrag", directory);
    snprintf(replacement, sizeof(replacement), "%s/replacement.gfrag", directory);
    const char *valid = "vec4(uv, 0.0, 1.0)";
    const char *changed = "vec4(uv, 1.0, 1.0)";
    const char *invalid = "uv.missing()";
    ViewSource source = {0};
    char error[256];
    CompileError compile_error;
    char *glsl = NULL;
    Image image = {0};
    GraficMesh mesh = {0};
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

    const char *gradient = "vec4(uv, 0.0, 1.0)";
    CHECK(write_source(path, gradient, strlen(gradient)));
    CHECK(view_source_poll(&source, path, error, sizeof(error)) == VIEW_SOURCE_CHANGED);
    RenderConfig config = {.width = 4, .height = 2, .seed = 42};
    CHECK(view_texture_render(&source, 0, &config, &image, &compile_error));
    CHECK(image.width == 4 && image.height == 2);
    CHECK(image_channel_to_byte(image_pixel(&image, 0, 0)[0]) == 32);
    CHECK(image_channel_to_byte(image_pixel(&image, 0, 0)[1]) == 64);
    CHECK(image_channel_to_byte(image_pixel(&image, 3, 1)[0]) == 223);
    CHECK(image_channel_to_byte(image_pixel(&image, 3, 1)[1]) == 191);
    float *previous = image.rgba;
    config.width = 0;
    CHECK(!view_texture_render(&source, 0, &config, &image, &compile_error));
    CHECK(image.rgba == previous && strcmp(compile_error.kind, "render") == 0);
    config.width = 4;
    CHECK(write_source(path, invalid, strlen(invalid)));
    CHECK(view_source_poll(&source, path, error, sizeof(error)) == VIEW_SOURCE_CHANGED);
    CHECK(!view_texture_render(&source, 0, &config, &image, &compile_error));
    CHECK(image.rgba == previous && strcmp(compile_error.kind, "semantic") == 0);

    const char *noise = "uv.multiply(4.5).noise().colorize(rgb(1.0, 1.0, 1.0))";
    CHECK(write_source(path, noise, strlen(noise)));
    CHECK(view_source_poll(&source, path, error, sizeof(error)) == VIEW_SOURCE_CHANGED);
    CHECK(view_texture_render(&source, 0, &config, &image, &compile_error));
    float pixels[4 * 2 * 4];
    memcpy(pixels, image.rgba, sizeof(pixels));
    CHECK(view_texture_render(&source, 0, &config, &image, &compile_error));
    CHECK(memcmp(pixels, image.rgba, sizeof(pixels)) == 0);
    config.seed = 43;
    CHECK(view_texture_render(&source, 0, &config, &image, &compile_error));
    CHECK(memcmp(pixels, image.rgba, sizeof(pixels)) != 0);

    struct {
        const char *text;
        size_t line;
        int red;
        int green;
    } previews[] = {
        {"0.25.show().missing()", 0, 64, 64},
        {"vec2(0.25, 0.75).show().missing()", 0, 64, 191},
        {"rgb(0.25, 0.75, 0.0).show().missing()", 0, 64, 191},
        {"vec4(0.25, 0.75, 0.0, 1.0).show().missing()", 0, 64, 191},
        {"0.25.show().add(0.5).show().missing()", 0, 191, 191},
        {"let v = 0.25;\nlet mask = v.show().missing();\nmask.missing()", 0, 64, 64},
        {"0.25\n.show()\n.add(0.5)\n.show()", 1, 64, 64},
        {"show(0.25).missing()", 0, 64, 64},
        {"0.25.add(show(0.5)).colorize(rgb(1.0, 1.0, 1.0))", 0, 128, 128},
    };
    for (size_t index = 0; index < sizeof(previews) / sizeof(previews[0]); index += 1) {
        ViewSource preview_source = {.text = (char *)previews[index].text,
            .length = strlen(previews[index].text), .available = true};
        CHECK(view_texture_render(&preview_source, previews[index].line, &config, &image, &compile_error));
        CHECK(image_channel_to_byte(image.rgba[0]) == previews[index].red);
        CHECK(image_channel_to_byte(image.rgba[1]) == previews[index].green);
    }
    const char *bad_show[] = {"uv.show(1.0)", "show()", "show(uv, uv)"};
    previous = image.rgba;
    for (size_t index = 0; index < sizeof(bad_show) / sizeof(bad_show[0]); index += 1) {
        ViewSource preview_source = {.text = (char *)bad_show[index],
            .length = strlen(bad_show[index]), .available = true};
        CHECK(!view_texture_render(&preview_source, 0, &config, &image, &compile_error));
        CHECK(strcmp(compile_error.kind, "semantic") == 0 && image.rgba == previous);
    }

    const char *mesh_source = "disk().segments(8).rings(2).show().segments(3)";
    ViewSource geometry = {.text = (char *)mesh_source, .length = strlen(mesh_source), .available = true};
    CHECK(view_mesh_render(&geometry, 0, &mesh, &compile_error));
    CHECK(mesh.vertex_count == 17 && mesh.index_count == 72);
    float *previous_positions = mesh.positions;
    const char *bad_meshes[] = {"disk().radius(-1.0)", "1.0.show()", "disk().missing()"};
    for (size_t index = 0; index < sizeof(bad_meshes) / sizeof(bad_meshes[0]); index += 1) {
        geometry.text = (char *)bad_meshes[index];
        geometry.length = strlen(geometry.text);
        CHECK(!view_mesh_render(&geometry, 0, &mesh, &compile_error));
        CHECK(mesh.positions == previous_positions && mesh.vertex_count == 17);
    }
    geometry.text = "disk().segments(8).rings(2)\n.show()\n.radius(-1.0)";
    geometry.length = strlen(geometry.text);
    CHECK(view_mesh_render(&geometry, 0, &mesh, &compile_error));
    CHECK(mesh.vertex_count == 17);
    CHECK(view_mesh_render(&geometry, 1, &mesh, &compile_error));
    CHECK(mesh.vertex_count == 17);
    geometry.available = false;
    previous_positions = mesh.positions;
    CHECK(!view_mesh_render(&geometry, 0, &mesh, &compile_error));
    CHECK(mesh.positions == previous_positions && strcmp(compile_error.kind, "read") == 0);

cleanup:
    graficmesh_mesh_destroy(&mesh);
    image_destroy(&image);
    free(glsl);
    view_source_destroy(&source);
    unlink(path);
    unlink(replacement);
    rmdir(directory);
    printf("%s viewer source lifecycle, compilation, and atomic texture/mesh generation\n", passed ? "PASS" : "FAIL");
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
