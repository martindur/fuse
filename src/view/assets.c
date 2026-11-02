#define _POSIX_C_SOURCE 200809L

#include "assets.h"
#include "gl/glsl_emit.h"
#include "gl/glsl_stdlib.h"
#include "tex/builtins.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void view_source_destroy(ViewSource *source)
{
    free(source->text);
    *source = (ViewSource){0};
}

ViewSourceStatus view_source_poll(ViewSource *source, const char *path, char *error, size_t capacity)
{
    struct stat status;
    if (stat(path, &status) != 0) {
        snprintf(error, capacity, "cannot read source: %s", strerror(errno));
        source->available = false;
        return VIEW_SOURCE_ERROR;
    }
    if (!S_ISREG(status.st_mode)) {
        snprintf(error, capacity, "source path must be a regular file");
        source->available = false;
        return VIEW_SOURCE_ERROR;
    }
    FILE *input = fopen(path, "rb");
    if (input == NULL) {
        snprintf(error, capacity, "cannot read source: %s", strerror(errno));
        source->available = false;
        return VIEW_SOURCE_ERROR;
    }
    char *text = NULL;
    size_t allocated = 0;
    errno = 0;
    ssize_t length = getdelim(&text, &allocated, '\0', input);
    bool failed = length < 0 && !feof(input);
    int saved_error = errno;
    if (fclose(input) != 0) {
        failed = true;
        saved_error = errno;
    }
    if (failed) {
        snprintf(error, capacity, "cannot read source: %s", strerror(saved_error == 0 ? EIO : saved_error));
        free(text);
        source->available = false;
        return VIEW_SOURCE_ERROR;
    }
    if (length < 0) {
        length = 0;
        if (text == NULL) {
            text = malloc(1);
        }
        if (text == NULL) {
            snprintf(error, capacity, "out of memory reading source");
            source->available = false;
            return VIEW_SOURCE_ERROR;
        }
        text[0] = '\0';
    }
    if (length > 0 && text[length - 1] == '\0') {
        snprintf(error, capacity, "NUL byte in source");
        free(text);
        source->available = false;
        return VIEW_SOURCE_ERROR;
    }
    if (source->available && source->length == (size_t)length
        && memcmp(source->text, text, (size_t)length) == 0) {
        free(text);
        return VIEW_SOURCE_UNCHANGED;
    }
    free(source->text);
    source->text = text;
    source->length = (size_t)length;
    source->available = true;
    return VIEW_SOURCE_CHANGED;
}

bool view_shader_compile(const ViewSource *source, size_t preview_line, char **out_glsl, CompileError *error)
{
    *out_glsl = NULL;
    if (!source->available) {
        *error = (CompileError){.kind = "read"};
        snprintf(error->message, sizeof(error->message), "shader source unavailable");
        return false;
    }
    PreparedProgram prepared = {0};
    if (!prepare_view_program(source->text, source->length, fusegl_dialect(), preview_line, &prepared, error)) {
        return false;
    }
    size_t length = 0;
    FILE *output = open_memstream(out_glsl, &length);
    bool emitted = output != NULL && glsl_emit(output, prepared.program, &prepared.semantic_info, source->text);
    if (output != NULL && fclose(output) != 0) {
        emitted = false;
    }
    prepared_program_destroy(&prepared);
    if (!emitted) {
        free(*out_glsl);
        *out_glsl = NULL;
        *error = (CompileError){.kind = "emit"};
        snprintf(error->message, sizeof(error->message), "cannot generate GLSL");
    }
    return emitted;
}

bool view_texture_render(const ViewSource *source, size_t preview_line, const RenderConfig *config, Image *out, CompileError *error)
{
    if (!source->available) {
        *error = (CompileError){.kind = "read"};
        snprintf(error->message, sizeof(error->message), "texture source unavailable");
        return false;
    }
    PreparedProgram prepared = {0};
    if (!prepare_view_program(source->text, source->length, fusetex_dialect(), preview_line, &prepared, error)) {
        return false;
    }
    Image candidate = {0};
    RenderError render_error;
    bool rendered = fusetex_render(prepared.program, source->text, config, &candidate, &render_error);
    prepared_program_destroy(&prepared);
    if (!rendered) {
        *error = (CompileError){.kind = "render", .span = render_error.span};
        snprintf(error->message, sizeof(error->message), "%s", render_error.message);
        image_destroy(&candidate);
        return false;
    }
    image_destroy(out);
    *out = candidate;
    return true;
}
