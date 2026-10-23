#include "fuse/help.h"

#include <string.h>

static bool print_signature(
    FILE *output,
    const BuiltinSignature *signature
)
{
    if (fprintf(output, "  %s(", signature->name) < 0) {
        return false;
    }

    for (size_t index = 0;
         index < signature->parameter_count;
         index += 1) {
        if ((index > 0 && fputs(", ", output) == EOF)
            || fputs(value_type_name(signature->parameters[index]), output)
                == EOF) {
            return false;
        }
    }

    return fprintf(
        output,
        ") -> %s\n",
        value_type_name(signature->result)
    ) >= 0;
}

static const char *documentation_for(
    const LanguageEnvironment *environment,
    const char *name
)
{
    for (size_t index = 0;
         index < environment->documentation_count;
         index += 1) {
        const SymbolDocumentation *documentation =
            &environment->documentation[index];

        if (strcmp(documentation->name, name) == 0) {
            return documentation->documentation;
        }
    }

    return NULL;
}

static bool name_was_printed(
    const LanguageEnvironment *environment,
    size_t end,
    const char *name
)
{
    for (size_t index = 0; index < end; index += 1) {
        if (strcmp(environment->builtins[index].name, name) == 0) {
            return true;
        }
    }

    return false;
}

void language_print_builtins(
    FILE *output,
    const LanguageEnvironment *environment
)
{
    if (output == NULL || environment == NULL) {
        return;
    }

    fputs("Builtins (signature types are shown in order):\n", output);

    for (size_t index = 0;
         index < environment->builtin_count;
         index += 1) {
        const BuiltinSignature *signature = &environment->builtins[index];

        if (!print_signature(output, signature)) {
            return;
        }

        if (!name_was_printed(
                environment,
                index,
                signature->name
            )) {
            const char *documentation = documentation_for(
                environment,
                signature->name
            );

            if (documentation != NULL) {
                fprintf(output, "    %s\n", documentation);
            }

        }
    }

    if (environment->global_count > 0) {
        fputs("\nGlobals:\n", output);

        for (size_t index = 0;
             index < environment->global_count;
             index += 1) {
            const GlobalSignature *global = &environment->globals[index];
            fprintf(
                output,
                "  %s: %s\n",
                global->name,
                value_type_name(global->type)
            );

            const char *documentation = documentation_for(
                environment,
                global->name
            );

            if (documentation != NULL) {
                fprintf(output, "    %s\n", documentation);
            }
        }
    }
}
