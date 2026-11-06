#define GL_GLEXT_PROTOTYPES
#define GLFW_INCLUDE_GLCOREARB
#include <GLFW/glfw3.h>

#include "render.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

static const char VERTEX_SHADER[] =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec2 a_uv;\n"
    "uniform mat4 u_model; uniform mat4 u_view; uniform mat4 u_projection;\n"
    "out vec2 graficUv; out vec3 graficPosition; out vec3 graficAuthoredNormal; flat out int graficHasNormal;\n"
    "void main() { graficAuthoredNormal = vec3(0.0); graficHasNormal = 0; graficUv = a_uv; graficPosition = a_position; gl_Position = u_projection * u_view * u_model * vec4(a_position, 1.0); }\n";

static const char FRAGMENT_SHADER[] =
    "#version 330 core\n"
    "in vec2 graficUv; uniform bool u_wireframe; out vec4 fragColor;\n"
    "void main() { fragColor = u_wireframe ? vec4(0.85, 0.85, 0.85, 1.0) : vec4(graficUv, 0.0, 1.0); }\n";

static void destroy_geometry(ViewMeshRenderer *mesh)
{
    glDeleteBuffers(3, mesh->buffers);
    glDeleteVertexArrays(1, &mesh->vertex_array);
}

void view_renderer_destroy_mesh(ViewRenderer *renderer)
{
    ViewMeshRenderer *mesh = &renderer->mesh;
    destroy_geometry(mesh);
    glDeleteProgram(mesh->program);
    glDeleteProgram(mesh->inspection_program);
    *mesh = (ViewMeshRenderer){0};
}

bool view_renderer_set_mesh_shaders(ViewRenderer *renderer, const char *vertex, const char *fragment,
    ViewRenderError *error)
{
    *error = (ViewRenderError){0};
    vertex = vertex == NULL ? VERTEX_SHADER : vertex;
    fragment = fragment == NULL ? FRAGMENT_SHADER : fragment;
    GLuint program = view_program_create(vertex, fragment, error);
    if (program == 0) return false;
    if (!view_program_bind_textures(renderer, program, error)) {
        glDeleteProgram(program);
        return false;
    }
    GLuint inspection = view_program_create(vertex, FRAGMENT_SHADER, error);
    if (inspection == 0) {
        glDeleteProgram(program);
        return false;
    }
    glDeleteProgram(renderer->mesh.program);
    glDeleteProgram(renderer->mesh.inspection_program);
    renderer->mesh.program = program;
    renderer->mesh.inspection_program = inspection;
    return true;
}

static bool mesh_bounds(const GraficMesh *mesh, float center[3], float *extent)
{
    if (mesh == NULL || mesh->positions == NULL || mesh->uvs == NULL || mesh->indices == NULL
        || mesh->vertex_count == 0 || mesh->index_count == 0 || mesh->index_count % 3 != 0
        || mesh->index_count > INT_MAX || mesh->index_count > PTRDIFF_MAX / sizeof(uint32_t)
        || mesh->vertex_count > PTRDIFF_MAX / (3 * sizeof(float))) return false;
    double minimum[3] = {INFINITY, INFINITY, INFINITY};
    double maximum[3] = {-INFINITY, -INFINITY, -INFINITY};
    for (size_t index = 0; index < mesh->vertex_count; index += 1) {
        for (size_t axis = 0; axis < 3; axis += 1) {
            double value = mesh->positions[index * 3 + axis];
            if (!isfinite(value)) return false;
            minimum[axis] = fmin(minimum[axis], value);
            maximum[axis] = fmax(maximum[axis], value);
        }
        if (!isfinite(mesh->uvs[index * 2]) || !isfinite(mesh->uvs[index * 2 + 1])) return false;
    }
    for (size_t index = 0; index < mesh->index_count; index += 1) {
        if (mesh->indices[index] >= mesh->vertex_count) return false;
    }
    double squared = 0;
    for (size_t axis = 0; axis < 3; axis += 1) {
        center[axis] = (float)((minimum[axis] + maximum[axis]) * 0.5);
        double half = (maximum[axis] - minimum[axis]) * 0.5;
        squared += half * half;
    }
    *extent = (float)sqrt(squared);
    return isfinite(*extent) && *extent > 0 && isfinite(1.0f / *extent);
}

bool view_renderer_set_mesh(ViewRenderer *renderer, const GraficMesh *mesh, ViewRenderError *error)
{
    *error = (ViewRenderError){0};
    ViewMeshRenderer candidate = {0};
    if (!mesh_bounds(mesh, candidate.center, &candidate.extent)) {
        snprintf(error->message, sizeof(error->message), "invalid mesh geometry for 3D view");
        return false;
    }
    candidate.index_count = mesh->index_count;
    GLint previous_array, previous_buffer;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_array);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_buffer);
    glGenVertexArrays(1, &candidate.vertex_array);
    glGenBuffers(3, candidate.buffers);
    bool allocated = candidate.vertex_array != 0 && candidate.buffers[0] != 0
        && candidate.buffers[1] != 0 && candidate.buffers[2] != 0;
    if (allocated) {
        glBindVertexArray(candidate.vertex_array);
        glBindBuffer(GL_ARRAY_BUFFER, candidate.buffers[0]);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(mesh->vertex_count * 3 * sizeof(float)), mesh->positions, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, candidate.buffers[1]);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(mesh->vertex_count * 2 * sizeof(float)), mesh->uvs, GL_STATIC_DRAW);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, candidate.buffers[2]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(mesh->index_count * sizeof(uint32_t)), mesh->indices, GL_STATIC_DRAW);
    }
    GLenum status = glGetError();
    glBindVertexArray((GLuint)previous_array);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)previous_buffer);
    if (!allocated || status != GL_NO_ERROR) {
        destroy_geometry(&candidate);
        snprintf(error->message, sizeof(error->message), "mesh upload failed (OpenGL error 0x%x)", status);
        return false;
    }
    if (renderer->mesh.program == 0 && !view_renderer_set_mesh_shaders(renderer, NULL, NULL, error)) {
        destroy_geometry(&candidate);
        return false;
    }
    candidate.program = renderer->mesh.program;
    candidate.inspection_program = renderer->mesh.inspection_program;
    destroy_geometry(&renderer->mesh);
    renderer->mesh = candidate;
    return true;
}

static void set_camera(GLuint program, const ViewMeshRenderer *mesh, int width, int height)
{
    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    /* Fixed isometric orientation, with framing derived only from source geometry. */
    float view[16] = {
        0.70710678f, -0.40824829f, 0.57735027f, 0,
        0,           0.81649658f, 0.57735027f, 0,
       -0.70710678f, -0.40824829f, 0.57735027f, 0,
        0,0,0,1,
    };
    for (size_t row = 0; row < 3; row += 1) {
        view[12 + row] = -(view[row] * mesh->center[0] + view[4 + row] * mesh->center[1]
            + view[8 + row] * mesh->center[2]);
    }
    float scale = 0.8f / mesh->extent;
    float projection[16] = {0};
    projection[0] = scale * (width > height ? (float)height / width : 1.0f);
    projection[5] = scale * (height > width ? (float)width / height : 1.0f);
    projection[10] = -0.125f / mesh->extent;
    projection[15] = 1;
    glUniformMatrix4fv(glGetUniformLocation(program, "u_model"), 1, GL_FALSE, identity);
    glUniformMatrix4fv(glGetUniformLocation(program, "u_view"), 1, GL_FALSE, view);
    glUniformMatrix4fv(glGetUniformLocation(program, "u_projection"), 1, GL_FALSE, projection);
    glUniform1i(glGetUniformLocation(program, "u_view_is_orthographic"), GL_TRUE);
    /* Camera +Z is toward the viewer; this row is expressed in mesh-local space. */
    glUniform3f(glGetUniformLocation(program, "u_view_direction"), view[2], view[6], view[10]);
}

void view_renderer_draw_mesh(const ViewRenderer *renderer, int width, int height, float time,
    bool wireframe, bool uv_colors)
{
    if (width <= 0 || height <= 0) return;
    const ViewMeshRenderer *mesh = &renderer->mesh;
    glViewport(0, 0, width, height);
    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (mesh->program == 0 || mesh->vertex_array == 0) return;
    GLuint program = wireframe || uv_colors ? mesh->inspection_program : mesh->program;
    glUseProgram(program);
    set_camera(program, mesh, width, height);
    glUniform1f(glGetUniformLocation(program, "u_time"), time);
    glUniform2f(glGetUniformLocation(program, "u_resolution"), (float)width, (float)height);
    glUniform1i(glGetUniformLocation(program, "u_wireframe"), wireframe);
    for (size_t slot = 0; slot < VIEW_TEXTURE_COUNT; slot += 1) {
        glActiveTexture((GLenum)(GL_TEXTURE0 + slot));
        glBindTexture(GL_TEXTURE_2D, renderer->textures[slot]);
    }
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(mesh->vertex_array);
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    glDrawElements(GL_TRIANGLES, (GLsizei)mesh->index_count, GL_UNSIGNED_INT, NULL);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_DEPTH_TEST);
}
