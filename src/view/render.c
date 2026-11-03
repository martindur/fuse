#define GL_GLEXT_PROTOTYPES
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include "render.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

const char *view_texture_name(size_t slot)
{
    static const char *const NAMES[] = {"tex0", "tex1", "tex2", "tex3"};
    return slot < VIEW_TEXTURE_COUNT ? NAMES[slot] : NULL;
}

static const char VERTEX_SHADER[] =
    "#version 330 core\n"
    "out vec2 graficUv;\n"
    "void main() {\n"
    "  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
    "  graficUv = p;\n"
    "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

static void window_error(ViewRenderError *error, const char *operation)
{
    const char *message = NULL;
    glfwGetError(&message);
    snprintf(error->message, sizeof(error->message), "%s: %s", operation,
        message == NULL ? "unknown windowing error" : message);
}

static GLuint compile_shader(GLenum type, const char *source, ViewRenderError *error)
{
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        snprintf(error->message, sizeof(error->message), "cannot allocate shader");
        return 0;
    }
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[3072] = {0};
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        snprintf(error->message, sizeof(error->message), "%s shader compilation failed: %s",
            type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint link_program(GLuint vertex_shader, const char *glsl, ViewRenderError *error)
{
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, glsl, error);
    if (fragment == 0) {
        return 0;
    }
    GLuint program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(fragment);
        snprintf(error->message, sizeof(error->message), "cannot allocate shader program");
        return 0;
    }
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDetachShader(program, fragment);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[3072] = {0};
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        snprintf(error->message, sizeof(error->message), "shader linking failed: %s", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

unsigned int view_program_create(const char *vertex, const char *fragment, ViewRenderError *error)
{
    GLuint shader = compile_shader(GL_VERTEX_SHADER, vertex, error);
    if (shader == 0) {
        return 0;
    }
    GLuint program = link_program(shader, fragment, error);
    glDeleteShader(shader);
    return program;
}

void view_renderer_destroy(ViewRenderer *renderer)
{
    if (renderer->window != NULL) {
        glfwMakeContextCurrent(renderer->window);
        if (renderer->program != 0) {
            glDeleteProgram(renderer->program);
        }
        if (renderer->texture_program != 0) {
            glDeleteProgram(renderer->texture_program);
        }
        if (renderer->vertex_shader != 0) {
            glDeleteShader(renderer->vertex_shader);
        }
        if (renderer->vertex_array != 0) {
            glDeleteVertexArrays(1, &renderer->vertex_array);
        }
        view_renderer_destroy_mesh(renderer);
        glDeleteTextures(VIEW_TEXTURE_COUNT, renderer->textures);
        glfwDestroyWindow(renderer->window);
    }
    if (renderer->initialized) {
        glfwTerminate();
    }
    *renderer = (ViewRenderer){0};
}

bool view_renderer_init(ViewRenderer *renderer, int width, int height, const char *title,
    bool visible, ViewRenderError *error)
{
    *error = (ViewRenderError){0};
    if (width <= 0 || height <= 0) {
        snprintf(error->message, sizeof(error->message), "window dimensions must be positive");
        return false;
    }
    if (!glfwInit()) {
        window_error(error, "cannot initialize windowing");
        return false;
    }
    renderer->initialized = true;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);
    renderer->window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (renderer->window == NULL) {
        window_error(error, "cannot create an OpenGL 3.3 window");
        view_renderer_destroy(renderer);
        return false;
    }
    glfwMakeContextCurrent(renderer->window);
    glfwSwapInterval(1);
    renderer->vertex_shader = compile_shader(GL_VERTEX_SHADER, VERTEX_SHADER, error);
    if (renderer->vertex_shader == 0) {
        view_renderer_destroy(renderer);
        return false;
    }
    renderer->texture_program = link_program(renderer->vertex_shader,
        "#version 330 core\nin vec2 graficUv; uniform sampler2D u_texture; out vec4 fragColor;\n"
        "void main() { fragColor = texture(u_texture, graficUv); }", error);
    if (renderer->texture_program == 0) {
        view_renderer_destroy(renderer);
        return false;
    }
    renderer->texture_uniform = glGetUniformLocation(renderer->texture_program, "u_texture");
    glGenVertexArrays(1, &renderer->vertex_array);
    if (renderer->vertex_array == 0) {
        snprintf(error->message, sizeof(error->message), "cannot allocate vertex array");
        view_renderer_destroy(renderer);
        return false;
    }
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &renderer->texture_limit);
    return true;
}

bool view_renderer_set_texture(ViewRenderer *renderer, size_t slot, const Image *image, ViewRenderError *error)
{
    *error = (ViewRenderError){0};
    if (slot >= VIEW_TEXTURE_COUNT || image == NULL || image->rgba == NULL
        || image->width == 0 || image->height == 0
        || image->width > (size_t)renderer->texture_limit || image->height > (size_t)renderer->texture_limit
        || image->width > SIZE_MAX / image->height || image->width * image->height > SIZE_MAX / 4) {
        snprintf(error->message, sizeof(error->message), "invalid texture or dimensions exceed GPU limit (%d)",
            renderer->texture_limit);
        return false;
    }
    size_t channels = image->width * image->height * 4;
    uint8_t *pixels = malloc(channels);
    if (pixels == NULL) {
        snprintf(error->message, sizeof(error->message), "out of memory preparing texture upload");
        return false;
    }
    for (size_t index = 0; index < channels; index += 1) {
        pixels[index] = image_channel_to_byte(image->rgba[index]);
    }

    GLint previous_unit;
    GLint previous_binding;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previous_unit);
    glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_binding);
    GLuint candidate = 0;
    glGenTextures(1, &candidate);
    glBindTexture(GL_TEXTURE_2D, candidate);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)image->width, (GLsizei)image->height,
        0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    free(pixels);
    GLenum status = glGetError();
    glBindTexture(GL_TEXTURE_2D, (GLuint)previous_binding);
    glActiveTexture((GLenum)previous_unit);
    if (candidate == 0 || status != GL_NO_ERROR) {
        glDeleteTextures(1, &candidate);
        snprintf(error->message, sizeof(error->message), "texture upload failed (OpenGL error 0x%x)", status);
        return false;
    }
    GLuint previous = renderer->textures[slot];
    renderer->textures[slot] = candidate;
    glDeleteTextures(1, &previous);
    return true;
}

bool view_renderer_set_shader(ViewRenderer *renderer, const char *glsl, ViewRenderError *error)
{
    *error = (ViewRenderError){0};
    GLuint program = link_program(renderer->vertex_shader, glsl, error);
    if (program == 0) {
        return false;
    }
    GLint samplers[VIEW_TEXTURE_COUNT];
    for (size_t slot = 0; slot < VIEW_TEXTURE_COUNT; slot += 1) {
        const char *name = view_texture_name(slot);
        samplers[slot] = glGetUniformLocation(program, name);
        if (samplers[slot] >= 0 && renderer->textures[slot] == 0) {
            snprintf(error->message, sizeof(error->message),
                "shader requires a loaded %s; provide --%s FILE.gtex and fix any texture errors", name, name);
            glDeleteProgram(program);
            return false;
        }
    }
    glUseProgram(program);
    for (size_t slot = 0; slot < VIEW_TEXTURE_COUNT; slot += 1) {
        glUniform1i(samplers[slot], (GLint)slot);
    }
    if (renderer->program != 0) {
        glDeleteProgram(renderer->program);
    }
    renderer->program = program;
    renderer->time_uniform = glGetUniformLocation(program, "u_time");
    renderer->resolution_uniform = glGetUniformLocation(program, "u_resolution");
    return true;
}

static void draw_output(const ViewRenderer *renderer, int width, int height, float time, int texture_slot)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    glViewport(0, 0, width, height);
    bool texture = texture_slot >= 0 && texture_slot < VIEW_TEXTURE_COUNT;
    if (texture ? renderer->textures[texture_slot] == 0 : renderer->program == 0) {
        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
    if (texture) {
        glUseProgram(renderer->texture_program);
        glUniform1i(renderer->texture_uniform, texture_slot);
    } else {
        glUseProgram(renderer->program);
        glUniform1f(renderer->time_uniform, time);
        glUniform2f(renderer->resolution_uniform, (float)width, (float)height);
    }
    for (size_t slot = 0; slot < VIEW_TEXTURE_COUNT; slot += 1) {
        glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
        glBindTexture(GL_TEXTURE_2D, renderer->textures[slot]);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(renderer->vertex_array);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void view_renderer_draw(const ViewRenderer *renderer, int width, int height, float time)
{
    draw_output(renderer, width, height, time, -1);
}

void view_renderer_draw_texture(const ViewRenderer *renderer, int width, int height, size_t slot)
{
    if (slot < VIEW_TEXTURE_COUNT) {
        draw_output(renderer, width, height, 0.0f, (int)slot);
    }
}
