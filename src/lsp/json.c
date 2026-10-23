#define _POSIX_C_SOURCE 200809L

#include "json.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void skip_space(
    const char *data,
    size_t length,
    size_t *offset
)
{
    while (*offset < length) {
        char character = data[*offset];

        if (character != ' '
            && character != '\t'
            && character != '\r'
            && character != '\n') {
            return;
        }

        *offset += 1;
    }
}

static bool skip_string(
    const char *data,
    size_t length,
    size_t *offset
)
{
    if (*offset >= length || data[*offset] != '"') {
        return false;
    }

    *offset += 1;

    while (*offset < length) {
        unsigned char character = (unsigned char)data[*offset];
        *offset += 1;

        if (character == '"') {
            return true;
        }

        if (character < 0x20) {
            return false;
        }

        if (character == '\\') {
            if (*offset >= length) {
                return false;
            }

            char escaped = data[*offset];
            *offset += 1;

            if (escaped == 'u') {
                if (length - *offset < 4) {
                    return false;
                }

                for (size_t index = 0; index < 4; index += 1) {
                    char digit = data[*offset + index];

                    if (!((digit >= '0' && digit <= '9')
                            || (digit >= 'a' && digit <= 'f')
                            || (digit >= 'A' && digit <= 'F'))) {
                        return false;
                    }
                }

                *offset += 4;
            } else if (escaped != '"'
                && escaped != '\\'
                && escaped != '/'
                && escaped != 'b'
                && escaped != 'f'
                && escaped != 'n'
                && escaped != 'r'
                && escaped != 't') {
                return false;
            }
        }
    }

    return false;
}

static bool skip_value(
    const char *data,
    size_t length,
    size_t *offset
);

static bool skip_object(
    const char *data,
    size_t length,
    size_t *offset
)
{
    if (*offset >= length || data[*offset] != '{') {
        return false;
    }

    *offset += 1;
    skip_space(data, length, offset);

    if (*offset < length && data[*offset] == '}') {
        *offset += 1;
        return true;
    }

    for (;;) {
        if (!skip_string(data, length, offset)) {
            return false;
        }

        skip_space(data, length, offset);

        if (*offset >= length || data[*offset] != ':') {
            return false;
        }

        *offset += 1;
        skip_space(data, length, offset);

        if (!skip_value(data, length, offset)) {
            return false;
        }

        skip_space(data, length, offset);

        if (*offset >= length) {
            return false;
        }

        if (data[*offset] == '}') {
            *offset += 1;
            return true;
        }

        if (data[*offset] != ',') {
            return false;
        }

        *offset += 1;
        skip_space(data, length, offset);
    }
}

static bool skip_array(
    const char *data,
    size_t length,
    size_t *offset
)
{
    if (*offset >= length || data[*offset] != '[') {
        return false;
    }

    *offset += 1;
    skip_space(data, length, offset);

    if (*offset < length && data[*offset] == ']') {
        *offset += 1;
        return true;
    }

    for (;;) {
        if (!skip_value(data, length, offset)) {
            return false;
        }

        skip_space(data, length, offset);

        if (*offset >= length) {
            return false;
        }

        if (data[*offset] == ']') {
            *offset += 1;
            return true;
        }

        if (data[*offset] != ',') {
            return false;
        }

        *offset += 1;
        skip_space(data, length, offset);
    }
}

static bool skip_number(
    const char *data,
    size_t length,
    size_t *offset
)
{
    size_t start = *offset;

    if (*offset < length && data[*offset] == '-') {
        *offset += 1;
    }

    if (*offset >= length) {
        return false;
    }

    if (data[*offset] == '0') {
        *offset += 1;
    } else {
        if (data[*offset] < '1' || data[*offset] > '9') {
            return false;
        }

        while (*offset < length
            && data[*offset] >= '0'
            && data[*offset] <= '9') {
            *offset += 1;
        }
    }

    if (*offset < length && data[*offset] == '.') {
        *offset += 1;
        size_t fraction = *offset;

        while (*offset < length
            && data[*offset] >= '0'
            && data[*offset] <= '9') {
            *offset += 1;
        }

        if (*offset == fraction) {
            return false;
        }
    }

    if (*offset < length
        && (data[*offset] == 'e' || data[*offset] == 'E')) {
        *offset += 1;

        if (*offset < length
            && (data[*offset] == '+' || data[*offset] == '-')) {
            *offset += 1;
        }

        size_t exponent = *offset;

        while (*offset < length
            && data[*offset] >= '0'
            && data[*offset] <= '9') {
            *offset += 1;
        }

        if (*offset == exponent) {
            return false;
        }
    }

    return *offset > start;
}

static bool literal(
    const char *data,
    size_t length,
    size_t *offset,
    const char *text
)
{
    size_t text_length = strlen(text);

    if (length - *offset < text_length
        || memcmp(data + *offset, text, text_length) != 0) {
        return false;
    }

    *offset += text_length;
    return true;
}

static bool skip_value(
    const char *data,
    size_t length,
    size_t *offset
)
{
    skip_space(data, length, offset);

    if (*offset >= length) {
        return false;
    }

    switch (data[*offset]) {
    case '{':
        return skip_object(data, length, offset);
    case '[':
        return skip_array(data, length, offset);
    case '"':
        return skip_string(data, length, offset);
    case 't':
        return literal(data, length, offset, "true");
    case 'f':
        return literal(data, length, offset, "false");
    case 'n':
        return literal(data, length, offset, "null");
    default:
        return skip_number(data, length, offset);
    }
}

FuseJsonType fuse_json_type(FuseJsonValue value)
{
    if (value.data == NULL || value.length == 0) {
        return FUSE_JSON_INVALID;
    }

    switch (value.data[value.offset]) {
    case 'n':
        return FUSE_JSON_NULL;
    case 't':
    case 'f':
        return FUSE_JSON_BOOL;
    case '"':
        return FUSE_JSON_STRING;
    case '{':
        return FUSE_JSON_OBJECT;
    case '[':
        return FUSE_JSON_ARRAY;
    default:
        return FUSE_JSON_NUMBER;
    }
}

bool fuse_json_parse(
    const char *data,
    size_t length,
    FuseJsonValue *out_value
)
{
    if (data == NULL || out_value == NULL) {
        return false;
    }

    size_t offset = 0;
    skip_space(data, length, &offset);
    size_t start = offset;

    if (!skip_value(data, length, &offset)) {
        return false;
    }

    skip_space(data, length, &offset);

    if (offset != length) {
        return false;
    }

    *out_value = (FuseJsonValue){
        .data = data,
        .length = offset - start,
        .offset = start,
    };
    return true;
}

static bool json_string_equals(
    FuseJsonValue value,
    const char *expected
)
{
    char *actual = NULL;
    bool equal = fuse_json_string_copy(value, &actual)
        && strcmp(actual, expected) == 0;
    free(actual);
    return equal;
}

bool fuse_json_object_get(
    FuseJsonValue object,
    const char *key,
    FuseJsonValue *out_value
)
{
    if (out_value == NULL
        || key == NULL
        || fuse_json_type(object) != FUSE_JSON_OBJECT) {
        return false;
    }

    size_t offset = object.offset + 1;
    size_t end = object.offset + object.length;
    skip_space(object.data, end, &offset);

    while (offset < end && object.data[offset] != '}') {
        size_t key_start = offset;

        if (!skip_string(object.data, end, &offset)) {
            return false;
        }

        FuseJsonValue key_value = {
            .data = object.data,
            .length = offset - key_start,
            .offset = key_start,
        };
        skip_space(object.data, end, &offset);

        if (offset >= end || object.data[offset] != ':') {
            return false;
        }

        offset += 1;
        skip_space(object.data, end, &offset);
        size_t value_start = offset;

        if (!skip_value(object.data, end, &offset)) {
            return false;
        }

        if (json_string_equals(key_value, key)) {
            *out_value = (FuseJsonValue){
                .data = object.data,
                .length = offset - value_start,
                .offset = value_start,
            };
            return true;
        }

        skip_space(object.data, end, &offset);

        if (offset >= end || object.data[offset] == '}') {
            break;
        }

        if (object.data[offset] != ',') {
            return false;
        }

        offset += 1;
        skip_space(object.data, end, &offset);
    }

    return false;
}

bool fuse_json_array_first(
    FuseJsonValue array,
    FuseJsonValue *out_value
)
{
    if (out_value == NULL
        || fuse_json_type(array) != FUSE_JSON_ARRAY) {
        return false;
    }

    size_t offset = array.offset + 1;
    size_t end = array.offset + array.length;
    skip_space(array.data, end, &offset);

    if (offset >= end || array.data[offset] == ']') {
        return false;
    }

    size_t start = offset;

    if (!skip_value(array.data, end, &offset)) {
        return false;
    }

    *out_value = (FuseJsonValue){
        .data = array.data,
        .length = offset - start,
        .offset = start,
    };
    return true;
}

static bool hex_digit(char character, unsigned int *out_value)
{
    if (character >= '0' && character <= '9') {
        *out_value = (unsigned int)(character - '0');
        return true;
    }

    if (character >= 'a' && character <= 'f') {
        *out_value = (unsigned int)(character - 'a') + 10;
        return true;
    }

    if (character >= 'A' && character <= 'F') {
        *out_value = (unsigned int)(character - 'A') + 10;
        return true;
    }

    return false;
}

static bool append_byte(
    char **buffer,
    size_t *length,
    size_t *capacity,
    unsigned char byte
)
{
    if (*length + 1 >= *capacity) {
        size_t next_capacity = *capacity == 0 ? 32 : *capacity * 2;

        if (next_capacity < *capacity) {
            return false;
        }

        char *expanded = realloc(*buffer, next_capacity);

        if (expanded == NULL) {
            return false;
        }

        *buffer = expanded;
        *capacity = next_capacity;
    }

    (*buffer)[*length] = (char)byte;
    *length += 1;
    return true;
}

static bool append_codepoint(
    char **buffer,
    size_t *length,
    size_t *capacity,
    uint32_t codepoint
)
{
    if (codepoint <= 0x7f) {
        return append_byte(buffer, length, capacity, (unsigned char)codepoint);
    }

    if (codepoint <= 0x7ff) {
        return append_byte(buffer, length, capacity,
            (unsigned char)(0xc0 | (codepoint >> 6)))
            && append_byte(buffer, length, capacity,
                (unsigned char)(0x80 | (codepoint & 0x3f)));
    }

    if (codepoint <= 0xffff) {
        return append_byte(buffer, length, capacity,
            (unsigned char)(0xe0 | (codepoint >> 12)))
            && append_byte(buffer, length, capacity,
                (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f)))
            && append_byte(buffer, length, capacity,
                (unsigned char)(0x80 | (codepoint & 0x3f)));
    }

    return append_byte(buffer, length, capacity,
        (unsigned char)(0xf0 | (codepoint >> 18)))
        && append_byte(buffer, length, capacity,
            (unsigned char)(0x80 | ((codepoint >> 12) & 0x3f)))
        && append_byte(buffer, length, capacity,
            (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f)))
        && append_byte(buffer, length, capacity,
            (unsigned char)(0x80 | (codepoint & 0x3f)));
}

bool fuse_json_string_copy(
    FuseJsonValue value,
    char **out_string
)
{
    if (out_string == NULL
        || fuse_json_type(value) != FUSE_JSON_STRING) {
        return false;
    }

    size_t offset = value.offset + 1;
    size_t end = value.offset + value.length - 1;
    char *result = NULL;
    size_t length = 0;
    size_t capacity = 0;

    while (offset < end) {
        unsigned char character = (unsigned char)value.data[offset++];

        if (character != '\\') {
            if (!append_byte(
                    &result,
                    &length,
                    &capacity,
                    character
                )) {
                free(result);
                return false;
            }
            continue;
        }

        if (offset >= end) {
            free(result);
            return false;
        }

        character = (unsigned char)value.data[offset++];

        switch (character) {
        case '"':
        case '\\':
        case '/':
            if (!append_byte(&result, &length, &capacity, character)) {
                free(result);
                return false;
            }
            break;
        case 'b':
            if (!append_byte(&result, &length, &capacity, '\b')) {
                free(result);
                return false;
            }
            break;
        case 'f':
            if (!append_byte(&result, &length, &capacity, '\f')) {
                free(result);
                return false;
            }
            break;
        case 'n':
            if (!append_byte(&result, &length, &capacity, '\n')) {
                free(result);
                return false;
            }
            break;
        case 'r':
            if (!append_byte(&result, &length, &capacity, '\r')) {
                free(result);
                return false;
            }
            break;
        case 't':
            if (!append_byte(&result, &length, &capacity, '\t')) {
                free(result);
                return false;
            }
            break;
        case 'u': {
            if (end - offset < 4) {
                free(result);
                return false;
            }

            uint32_t codepoint = 0;

            for (size_t index = 0; index < 4; index += 1) {
                unsigned int digit;

                if (!hex_digit(value.data[offset + index], &digit)) {
                    free(result);
                    return false;
                }

                codepoint = (codepoint << 4) | digit;
            }

            offset += 4;

            if (!append_codepoint(
                    &result,
                    &length,
                    &capacity,
                    codepoint
                )) {
                free(result);
                return false;
            }
            break;
        }
        default:
            free(result);
            return false;
        }
    }

    if (!append_byte(&result, &length, &capacity, '\0')) {
        free(result);
        return false;
    }

    *out_string = result;
    return true;
}
