#define GL_GLEXT_PROTOTYPES
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include "render.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static const char VERTEX_SHADER[] =
    "#version 330 core\n"
    "layout(location = 0) in vec2 position;\n"
    "layout(location = 1) in vec2 uv;\n"
    "uniform vec2 u_aspect; out vec2 meshUv;\n"
    "void main() { meshUv = uv; gl_Position = vec4(position * u_aspect * 0.9, 0.0, 1.0); }\n";

static const char FRAGMENT_SHADER[] =
    "#version 330 core\n"
    "in vec2 meshUv; uniform bool u_wireframe; out vec4 fragColor;\n"
    "void main() { fragColor = u_wireframe ? vec4(0.85, 0.85, 0.85, 1.0) : vec4(meshUv, 0.0, 1.0); }\n";

static void destroy_mesh(ViewMeshRenderer *mesh)
{
    glDeleteBuffers(3, mesh->buffers);
    glDeleteVertexArrays(1, &mesh->vertex_array);
    if (mesh->program != 0) {
        glDeleteProgram(mesh->program);
    }
    *mesh = (ViewMeshRenderer){0};
}

void view_renderer_destroy_mesh(ViewRenderer *renderer)
{
    destroy_mesh(&renderer->mesh);
}

static bool mesh_bounds(const FuseMesh *mesh, double center[2], double *extent)
{
    if (mesh == NULL || mesh->positions == NULL || mesh->uvs == NULL || mesh->indices == NULL
        || mesh->vertex_count == 0 || mesh->index_count == 0 || mesh->index_count % 3 != 0
        || mesh->index_count > INT_MAX || mesh->index_count > PTRDIFF_MAX / sizeof(uint32_t)
        || mesh->vertex_count > PTRDIFF_MAX / (3 * sizeof(float))) {
        return false;
    }
    double minimum[2] = {INFINITY, INFINITY};
    double maximum[2] = {-INFINITY, -INFINITY};
    for (size_t index = 0; index < mesh->vertex_count; index += 1) {
        for (size_t axis = 0; axis < 3; axis += 1) {
            if (!isfinite(mesh->positions[index * 3 + axis])) {
                return false;
            }
        }
        for (size_t axis = 0; axis < 2; axis += 1) {
            if (!isfinite(mesh->uvs[index * 2 + axis])) {
                return false;
            }
            double value = mesh->positions[index * 3 + axis];
            minimum[axis] = fmin(minimum[axis], value);
            maximum[axis] = fmax(maximum[axis], value);
        }
    }
    for (size_t index = 0; index < mesh->index_count; index += 1) {
        if (mesh->indices[index] >= mesh->vertex_count) {
            return false;
        }
    }
    for (size_t axis = 0; axis < 2; axis += 1) {
        center[axis] = (minimum[axis] + maximum[axis]) * 0.5;
    }
    *extent = fmax(maximum[0] - minimum[0], maximum[1] - minimum[1]) * 0.5;
    return *extent > 0.0;
}

bool view_renderer_set_mesh(ViewRenderer *renderer, const FuseMesh *mesh, ViewRenderError *error)
{
    *error = (ViewRenderError){0};
    double center[2];
    double extent;
    if (!mesh_bounds(mesh, center, &extent)) {
        snprintf(error->message, sizeof(error->message), "invalid mesh geometry for front view");
        return false;
    }
    float *positions = malloc(mesh->vertex_count * 2 * sizeof(*positions));
    if (positions == NULL) {
        snprintf(error->message, sizeof(error->message), "out of memory preparing mesh upload");
        return false;
    }
    for (size_t index = 0; index < mesh->vertex_count; index += 1) {
        for (size_t axis = 0; axis < 2; axis += 1) {
            positions[index * 2 + axis] = (float)((mesh->positions[index * 3 + axis] - center[axis]) / extent);
        }
    }
    ViewMeshRenderer candidate = {0};
    candidate.program = view_program_create(VERTEX_SHADER, FRAGMENT_SHADER, error);
    if (candidate.program == 0) {
        free(positions);
        return false;
    }
    candidate.aspect_uniform = glGetUniformLocation(candidate.program, "u_aspect");
    candidate.wireframe_uniform = glGetUniformLocation(candidate.program, "u_wireframe");
    candidate.index_count = mesh->index_count;

    GLint previous_array;
    GLint previous_buffer;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_array);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_buffer);
    glGenVertexArrays(1, &candidate.vertex_array);
    glGenBuffers(3, candidate.buffers);
    bool allocated = candidate.vertex_array != 0 && candidate.buffers[0] != 0
        && candidate.buffers[1] != 0 && candidate.buffers[2] != 0;
    if (allocated) {
        glBindVertexArray(candidate.vertex_array);
        glBindBuffer(GL_ARRAY_BUFFER, candidate.buffers[0]);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(mesh->vertex_count * 2 * sizeof(float)), positions, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, candidate.buffers[1]);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(mesh->vertex_count * 2 * sizeof(float)), mesh->uvs, GL_STATIC_DRAW);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, candidate.buffers[2]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(mesh->index_count * sizeof(uint32_t)), mesh->indices, GL_STATIC_DRAW);
    }
    free(positions);
    GLenum status = glGetError();
    glBindVertexArray((GLuint)previous_array);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)previous_buffer);
    if (!allocated || status != GL_NO_ERROR) {
        destroy_mesh(&candidate);
        snprintf(error->message, sizeof(error->message), "mesh upload failed (OpenGL error 0x%x)", status);
        return false;
    }
    destroy_mesh(&renderer->mesh);
    renderer->mesh = candidate;
    return true;
}

void view_renderer_draw_mesh(const ViewRenderer *renderer, int width, int height, bool wireframe)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    glViewport(0, 0, width, height);
    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    const ViewMeshRenderer *mesh = &renderer->mesh;
    if (mesh->program == 0) {
        return;
    }
    glUseProgram(mesh->program);
    glUniform2f(mesh->aspect_uniform, width > height ? (float)height / width : 1.0f,
        height > width ? (float)width / height : 1.0f);
    glUniform1i(mesh->wireframe_uniform, wireframe);
    glBindVertexArray(mesh->vertex_array);
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    glDrawElements(GL_TRIANGLES, (GLsizei)mesh->index_count, GL_UNSIGNED_INT, NULL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}
