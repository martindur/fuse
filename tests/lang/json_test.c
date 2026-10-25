#include "json.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool test_nested_json_values(void)
{
    const char source[] =
        "{\"params\":{\"text\":\"line\\n\\u00e5\"},"
        "\"changes\":[{\"text\":\"uv\"}]}";
    FuseJsonValue root;
    FuseJsonValue params;
    FuseJsonValue changes;
    FuseJsonValue change;
    FuseJsonValue text_value;
    FuseJsonValue change_text;
    char *text = NULL;
    char *change_string = NULL;
    bool passed = fuse_json_parse(
        source,
        sizeof(source) - 1,
        &root
    )
        && fuse_json_object_get(root, "params", &params)
        && fuse_json_object_get(params, "text", &text_value)
        && fuse_json_object_get(root, "changes", &changes)
        && fuse_json_array_first(changes, &change)
        && fuse_json_string_copy(text_value, &text)
        && strcmp(text, "line\n\xc3\xa5") == 0
        && fuse_json_object_get(change, "text", &change_text)
        && fuse_json_string_copy(change_text, &change_string)
        && strcmp(change_string, "uv") == 0;

    free(text);
    free(change_string);

    if (!passed) {
        fprintf(stderr, "FAIL JSON nested values\n");
    } else {
        printf("PASS JSON nested values\n");
    }

    return passed;
}

int main(void)
{
    if (test_nested_json_values()) {
        printf("1/1 Fuse JSON tests passed\n");
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
