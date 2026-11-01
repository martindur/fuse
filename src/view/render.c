#define GL_GLEXT_PROTOTYPES
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include "render.h"

#include <stdio.h>

static const char VERTEX_SHADER[] =
    "#version 330 core\n"
    "out vec2 fuseUv;\n"
    "void main() {\n"
    "  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
    "  fuseUv = p;\n"
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

void view_renderer_destroy(ViewRenderer *renderer)
{
    if (renderer->window != NULL) {
        glfwMakeContextCurrent(renderer->window);
        if (renderer->program != 0) {
            glDeleteProgram(renderer->program);
        }
        if (renderer->vertex_shader != 0) {
            glDeleteShader(renderer->vertex_shader);
        }
        if (renderer->vertex_array != 0) {
            glDeleteVertexArrays(1, &renderer->vertex_array);
        }
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
    glGenVertexArrays(1, &renderer->vertex_array);
    if (renderer->vertex_array == 0) {
        snprintf(error->message, sizeof(error->message), "cannot allocate vertex array");
        view_renderer_destroy(renderer);
        return false;
    }
    return true;
}

bool view_renderer_set_shader(ViewRenderer *renderer, const char *glsl, ViewRenderError *error)
{
    *error = (ViewRenderError){0};
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, glsl, error);
    if (fragment == 0) {
        return false;
    }
    GLuint program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(fragment);
        snprintf(error->message, sizeof(error->message), "cannot allocate shader program");
        return false;
    }
    glAttachShader(program, renderer->vertex_shader);
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
        return false;
    }
    static const char *const TEXTURE_INPUTS[] = {"tex0", "tex1", "tex2", "tex3"};
    for (size_t slot = 0; slot < sizeof(TEXTURE_INPUTS) / sizeof(TEXTURE_INPUTS[0]); slot += 1) {
        const char *name = TEXTURE_INPUTS[slot];
        if (glGetUniformLocation(program, name) >= 0) {
            snprintf(error->message, sizeof(error->message),
                "shader requires %s; texture inputs are not available in fuse view yet", name);
            glDeleteProgram(program);
            return false;
        }
    }
    if (renderer->program != 0) {
        glDeleteProgram(renderer->program);
    }
    renderer->program = program;
    renderer->time_uniform = glGetUniformLocation(program, "u_time");
    renderer->resolution_uniform = glGetUniformLocation(program, "u_resolution");
    return true;
}

void view_renderer_draw(const ViewRenderer *renderer, int width, int height, float time)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    glViewport(0, 0, width, height);
    if (renderer->program == 0) {
        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }
    glUseProgram(renderer->program);
    glUniform1f(renderer->time_uniform, time);
    glUniform2f(renderer->resolution_uniform, (float)width, (float)height);
    glBindVertexArray(renderer->vertex_array);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
