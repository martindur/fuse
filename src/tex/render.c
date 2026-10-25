#include "render.h"

bool fusetex_render(
    const Program *program,
    const char *source,
    const RenderConfig *config,
    Image *out_image,
    RenderError *out_error
)
{
    if (out_image == NULL || out_error == NULL) {
        return false;
    }

    *out_error = (RenderError){0};

    if (program == NULL
        || program->result == NULL
        || source == NULL
        || config == NULL
        || config->width == 0
        || config->height == 0) {
        out_error->message = "invalid render input";
        return false;
    }

    if (!image_init(out_image, config->width, config->height)) {
        out_error->message = "out of memory";
        return false;
    }

    EvalContext context = {
        .resolution = {
            (float)config->width,
            (float)config->height,
        },
        .seed = config->seed,
    };

    for (size_t y = 0; y < config->height; y += 1) {
        for (size_t x = 0; x < config->width; x += 1) {
            context.uv[0] = ((float)x + 0.5f)
                / (float)config->width;
            context.uv[1] = ((float)y + 0.5f)
                / (float)config->height;

            RuntimeValue value;
            EvalError eval_error;

            if (!fusetex_evaluate_program(
                    program,
                    source,
                    &context,
                    &value,
                    &eval_error
                )) {
                out_error->span = eval_error.span;
                out_error->message = eval_error.message;
                image_destroy(out_image);
                return false;
            }

            if (!image_store_value(out_image, x, y, &value)) {
                out_error->span = program->result->span;
                out_error->message =
                    "program result cannot be written as RGBA";
                image_destroy(out_image);
                return false;
            }
        }
    }

    return true;
}
