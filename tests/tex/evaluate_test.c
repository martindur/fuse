#define _POSIX_C_SOURCE 200809L

#include "tex/builtins.h"
#include "tex/image.h"
#include "fuse/lower.h"
#include "fuse/parser.h"
#include "tex/png_write.h"
#include "tex/render.h"
#include "fuse/semantic.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Program *parse_and_lower(const char *source)
{
    Program *syntax = NULL;
    ParseError parse_error;

    if (!parse_program(
            source,
            strlen(source),
            &syntax,
            &parse_error
        )) {
        fprintf(
            stderr,
            "test setup parse error at %zu:%zu: %s\n",
            parse_error.span.offset,
            parse_error.span.length,
            parse_error.message
        );
        return NULL;
    }

    Program *lowered = NULL;
    LowerError lower_error;

    if (!lower_program(syntax, &lowered, &lower_error)) {
        fprintf(
            stderr,
            "test setup lowering error at %zu:%zu: %s\n",
            lower_error.span.offset,
            lower_error.span.length,
            lower_error.message
        );
    }

    program_destroy(syntax);
    return lowered;
}

static bool analyze_program(
    Program *program,
    const char *source,
    SemanticInfo *out_info
)
{
    SemanticError error;

    if (!semantic_analyze(
            program,
            source,
            fusetex_dialect()->environment,
            out_info,
            &error
        )) {
        fprintf(
            stderr,
            "FAIL FuseTex semantic analysis at %zu:%zu: %s\n",
            error.span.offset,
            error.span.length,
            error.message
        );
        return false;
    }

    return true;
}

static bool evaluate_source_at(
    const char *source,
    float uv_x,
    float uv_y,
    uint64_t seed,
    RuntimeValue *out_value
)
{
    Program *program = parse_and_lower(source);

    if (program == NULL) {
        return false;
    }

    SemanticInfo semantic_info;

    if (!analyze_program(program, source, &semantic_info)) {
        program_destroy(program);
        return false;
    }

    EvalContext context = {
        .uv = {uv_x, uv_y},
        .resolution = {2.0f, 2.0f},
        .seed = seed,
    };
    EvalError error;
    bool evaluated = fusetex_evaluate_program(
        program,
        source,
        &context,
        out_value,
        &error
    );

    if (!evaluated) {
        fprintf(
            stderr,
            "FAIL FuseTex primitive evaluation at %zu:%zu: %s\n",
            error.span.offset,
            error.span.length,
            error.message
        );
    }

    semantic_info_destroy(&semantic_info);
    program_destroy(program);
    return evaluated;
}

static bool approximately_equal(float actual, float expected)
{
    float difference = actual - expected;

    return difference > -0.0001f && difference < 0.0001f;
}

static bool test_gradient_render(void)
{
    const char *source = "uv.vec4(0.0, 1.0)";
    Program *program = parse_and_lower(source);

    if (program == NULL) {
        return false;
    }

    SemanticInfo semantic_info;

    if (!analyze_program(program, source, &semantic_info)) {
        program_destroy(program);
        return false;
    }

    RenderConfig config = {
        .width = 2,
        .height = 2,
        .seed = 7,
    };
    Image image = {0};
    RenderError render_error;
    bool rendered = fusetex_render(
        program,
        source,
        &config,
        &image,
        &render_error
    );
    bool passed = rendered;

    if (!rendered) {
        fprintf(
            stderr,
            "FAIL FuseTex gradient render at %zu:%zu: %s\n",
            render_error.span.offset,
            render_error.span.length,
            render_error.message
        );
    } else {
        const float *bottom_left = image_pixel(&image, 0, 0);
        const float *top_right = image_pixel(&image, 1, 1);

        passed = bottom_left != NULL
            && top_right != NULL
            && approximately_equal(bottom_left[0], 0.25f)
            && approximately_equal(bottom_left[1], 0.25f)
            && approximately_equal(bottom_left[2], 0.0f)
            && approximately_equal(bottom_left[3], 1.0f)
            && approximately_equal(top_right[0], 0.75f)
            && approximately_equal(top_right[1], 0.75f)
            && approximately_equal(top_right[2], 0.0f)
            && approximately_equal(top_right[3], 1.0f);

        if (!passed) {
            fprintf(stderr, "FAIL FuseTex gradient pixel values\n");
        }

        image_destroy(&image);
    }

    semantic_info_destroy(&semantic_info);
    program_destroy(program);

    if (passed) {
        printf("PASS FuseTex gradient render\n");
    }

    return passed;
}

static bool test_texture_primitives(void)
{
    RuntimeValue radial;
    RuntimeValue vectors;
    RuntimeValue scalar;
    RuntimeValue smooth;
    RuntimeValue first_noise;
    RuntimeValue second_noise;
    RuntimeValue periodic_noise;
    RuntimeValue periodic_noise_x;
    RuntimeValue periodic_noise_y;
    bool radial_evaluated = evaluate_source_at(
        "uv.center().aspect().length()"
        " .multiply(4.0).sin().abs()"
        " .colorize(rgb(1.0, 1.0, 1.0))",
        0.75f,
        0.5f,
        7,
        &radial
    );
    bool vectors_evaluated = evaluate_source_at(
        "uv.multiply(2.0).add(vec2(0.25, 0.5))"
        " .dot(vec2(1.0, -1.0))"
        " .colorize(rgb(1.0, 1.0, 1.0))",
        0.25f,
        0.75f,
        7,
        &vectors
    );
    bool scalar_evaluated = evaluate_source_at(
        "let cosine = 0.25.cos();"
        " let fraction = 1.75.floor().fract();"
        " cosine.add(fraction).colorize(rgb(1.0, 1.0, 1.0))",
        0.25f,
        0.75f,
        7,
        &scalar
    );
    bool smooth_evaluated = evaluate_source_at(
        "0.5.smoothstep(0.0, 1.0)"
        " .colorize(rgb(1.0, 1.0, 1.0))",
        0.25f,
        0.75f,
        7,
        &smooth
    );
    bool first_noise_evaluated = evaluate_source_at(
        "uv.multiply(4.0).noise()"
        " .colorize(rgb(1.0, 1.0, 1.0))",
        0.37f,
        0.63f,
        7,
        &first_noise
    );
    bool second_noise_evaluated = evaluate_source_at(
        "uv.multiply(4.0).noise()"
        " .colorize(rgb(1.0, 1.0, 1.0))",
        0.37f,
        0.63f,
        7,
        &second_noise
    );
    bool periodic_noise_evaluated = evaluate_source_at(
        "uv.multiply(4.0).noise(vec2(4.0, 4.0))"
        " .colorize(rgb(1.0, 1.0, 1.0))",
        0.37f,
        0.63f,
        7,
        &periodic_noise
    );
    bool periodic_noise_x_evaluated = evaluate_source_at(
        "uv.multiply(4.0).noise(vec2(4.0, 4.0))"
        " .colorize(rgb(1.0, 1.0, 1.0))",
        1.37f,
        0.63f,
        7,
        &periodic_noise_x
    );
    bool periodic_noise_y_evaluated = evaluate_source_at(
        "uv.multiply(4.0).noise(vec2(4.0, 4.0))"
        " .colorize(rgb(1.0, 1.0, 1.0))",
        0.37f,
        1.63f,
        7,
        &periodic_noise_y
    );
    bool passed = radial_evaluated
        && radial.type == VALUE_TYPE_VEC4
        && approximately_equal(radial.components[3], sinf(1.0f))
        && vectors_evaluated
        && vectors.type == VALUE_TYPE_VEC4
        && approximately_equal(vectors.components[3], -1.25f)
        && scalar_evaluated
        && scalar.type == VALUE_TYPE_VEC4
        && approximately_equal(scalar.components[0], cosf(0.25f))
        && smooth_evaluated
        && smooth.type == VALUE_TYPE_VEC4
        && approximately_equal(smooth.components[3], 0.5f)
        && first_noise_evaluated
        && second_noise_evaluated
        && first_noise.type == VALUE_TYPE_VEC4
        && approximately_equal(
            first_noise.components[3],
            second_noise.components[3]
        )
        && first_noise.components[3] >= 0.0f
        && first_noise.components[3] <= 1.0f
        && periodic_noise_evaluated
        && periodic_noise_x_evaluated
        && periodic_noise_y_evaluated
        && approximately_equal(
            periodic_noise.components[3],
            periodic_noise_x.components[3]
        )
        && approximately_equal(
            periodic_noise.components[3],
            periodic_noise_y.components[3]
        );

    if (!passed) {
        fprintf(stderr, "FAIL FuseTex texture primitives\n");
    } else {
        printf("PASS FuseTex texture primitives\n");
    }

    return passed;
}

static bool test_image_reinitialization(void)
{
    Image image = {0};
    RuntimeValue value = {
        .type = VALUE_TYPE_VEC4,
        .components = {0.2f, 0.4f, 0.8f, 1.0f},
    };

    bool initialized = image_init(&image, 1, 1);
    bool stored = initialized && image_store_value(&image, 0, 0, &value);
    bool rejected = !image_init(&image, SIZE_MAX, 2);
    const float *preserved = image_pixel(&image, 0, 0);
    bool preserved_value = preserved != NULL
        && approximately_equal(preserved[0], 0.2f)
        && approximately_equal(preserved[1], 0.4f)
        && approximately_equal(preserved[2], 0.8f)
        && approximately_equal(preserved[3], 1.0f);
    bool reinitialized = image_init(&image, 2, 1);
    const float *cleared = image_pixel(&image, 1, 0);
    bool passed = stored
        && rejected
        && preserved_value
        && reinitialized
        && image.width == 2
        && image.height == 1
        && cleared != NULL
        && approximately_equal(cleared[0], 0.0f)
        && approximately_equal(cleared[1], 0.0f)
        && approximately_equal(cleared[2], 0.0f)
        && approximately_equal(cleared[3], 0.0f);

    image_destroy(&image);

    if (!passed) {
        fprintf(stderr, "FAIL FuseTex image reinitialization\n");
    } else {
        printf("PASS FuseTex image reinitialization\n");
    }

    return passed;
}

static bool test_preview_value_storage(void)
{
    Image image = {0};
    RuntimeValue value = {
        .type = VALUE_TYPE_VEC2,
        .components = {0.25f, 0.75f},
    };
    bool stored = image_init(&image, 1, 1)
        && image_store_value(&image, 0, 0, &value);
    const float *pixel = image_pixel(&image, 0, 0);
    bool passed = stored
        && pixel != NULL
        && approximately_equal(pixel[0], 0.25f)
        && approximately_equal(pixel[1], 0.75f)
        && approximately_equal(pixel[2], 0.0f)
        && approximately_equal(pixel[3], 1.0f);
    image_destroy(&image);

    if (!passed) {
        fprintf(stderr, "FAIL FuseTex preview value storage\n");
    } else {
        printf("PASS FuseTex preview value storage\n");
    }

    return passed;
}

static bool test_png_signature(void)
{
    Image image = {0};

    if (!image_init(&image, 1, 1)) {
        return false;
    }

    RuntimeValue value = {
        .type = VALUE_TYPE_VEC4,
        .components = {0.2f, 0.4f, 0.8f, 1.0f},
    };
    bool stored = image_store_value(&image, 0, 0, &value);
    FILE *output = tmpfile();

    if (!stored || output == NULL) {
        if (output != NULL) {
            fclose(output);
        }
        image_destroy(&image);
        return false;
    }

    bool written = png_write(output, &image);
    uint8_t signature[8] = {0};

    if (written && (fflush(output) == EOF || fseek(output, 0, SEEK_SET) != 0)) {
        written = false;
    }

    if (written && fread(signature, 1, sizeof(signature), output) != sizeof(signature)) {
        written = false;
    }

    static const uint8_t PNG_SIGNATURE[] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
    };
    bool passed = written
        && memcmp(signature, PNG_SIGNATURE, sizeof(signature)) == 0;

    fclose(output);
    image_destroy(&image);

    if (!passed) {
        fprintf(stderr, "FAIL FuseTex PNG signature\n");
    } else {
        printf("PASS FuseTex PNG signature\n");
    }

    return passed;
}

int main(void)
{
    size_t passed = 0;

    if (test_gradient_render()) {
        passed += 1;
    }

    if (test_texture_primitives()) {
        passed += 1;
    }

    if (test_image_reinitialization()) {
        passed += 1;
    }

    if (test_png_signature()) {
        passed += 1;
    }

    if (test_preview_value_storage()) {
        passed += 1;
    }

    printf("%zu/5 FuseTex tests passed\n", passed);
    return passed == 5 ? EXIT_SUCCESS : EXIT_FAILURE;
}
