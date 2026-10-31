#define _POSIX_C_SOURCE 200809L

#include "fuse/lsp.h"

#include "json.h"

#include "fuse/lexer.h"
#include "fuse/lower.h"
#include "fuse/parser.h"
#include "fuse/semantic.h"
#include "fuse/value_type.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *uri;
    char *text;
    const FuseDialect *dialect;
    int64_t version;
} FuseLspDocument;

typedef struct {
    const FuseDialect *dialect;
    FuseLspDocument *documents;
    size_t document_count;
    size_t document_capacity;
    FILE *output;
    FILE *log;
    bool shutdown_requested;
    bool exit_requested;
} FuseLspServer;

typedef struct {
    SourceSpan span;
    const char *message;
    char result_message[64];
} FuseLspIssue;

typedef struct {
    size_t line;
    size_t character;
} FuseLspPosition;

static void document_destroy(FuseLspDocument *document)
{
    if (document == NULL) {
        return;
    }

    free(document->uri);
    free(document->text);
    *document = (FuseLspDocument){0};
}

static FuseLspDocument *find_document(
    FuseLspServer *server,
    const char *uri
)
{
    for (size_t index = 0; index < server->document_count; index += 1) {
        if (strcmp(server->documents[index].uri, uri) == 0) {
            return &server->documents[index];
        }
    }

    return NULL;
}

static bool json_get_string(
    FuseJsonValue object,
    const char *key,
    char **out_string
)
{
    FuseJsonValue value;

    return fuse_json_object_get(object, key, &value)
        && fuse_json_string_copy(value, out_string);
}

static bool json_get_object(
    FuseJsonValue object,
    const char *key,
    FuseJsonValue *out_value
)
{
    return fuse_json_object_get(object, key, out_value)
        && fuse_json_type(*out_value) == FUSE_JSON_OBJECT;
}

static bool json_get_array(
    FuseJsonValue object,
    const char *key,
    FuseJsonValue *out_value
)
{
    return fuse_json_object_get(object, key, out_value)
        && fuse_json_type(*out_value) == FUSE_JSON_ARRAY;
}

static bool json_get_size(
    FuseJsonValue object,
    const char *key,
    size_t *out_value
)
{
    FuseJsonValue value;

    if (!fuse_json_object_get(object, key, &value)
        || fuse_json_type(value) != FUSE_JSON_NUMBER
        || value.length == 0) {
        return false;
    }

    char *text = malloc(value.length + 1);

    if (text == NULL) {
        return false;
    }

    memcpy(text, value.data + value.offset, value.length);
    text[value.length] = '\0';

    char *end = NULL;
    errno = 0;
    unsigned long long parsed = strtoull(text, &end, 10);
    bool valid = errno == 0
        && end != text
        && *end == '\0'
        && parsed <= SIZE_MAX;

    if (valid) {
        *out_value = (size_t)parsed;
    }

    free(text);
    return valid;
}

static bool json_get_position(
    FuseJsonValue object,
    FuseLspPosition *out_position
)
{
    FuseJsonValue position;

    return json_get_object(object, "position", &position)
        && json_get_size(position, "line", &out_position->line)
        && json_get_size(
            position,
            "character",
            &out_position->character
        );
}

static bool write_json_string(FILE *output, const char *string)
{
    if (fputc('"', output) == EOF) {
        return false;
    }

    for (const unsigned char *character = (const unsigned char *)string;
         *character != '\0';
         character += 1) {
        switch (*character) {
        case '"':
            if (fputs("\\\"", output) == EOF) {
                return false;
            }
            break;
        case '\\':
            if (fputs("\\\\", output) == EOF) {
                return false;
            }
            break;
        case '\b':
            if (fputs("\\b", output) == EOF) {
                return false;
            }
            break;
        case '\f':
            if (fputs("\\f", output) == EOF) {
                return false;
            }
            break;
        case '\n':
            if (fputs("\\n", output) == EOF) {
                return false;
            }
            break;
        case '\r':
            if (fputs("\\r", output) == EOF) {
                return false;
            }
            break;
        case '\t':
            if (fputs("\\t", output) == EOF) {
                return false;
            }
            break;
        default:
            if (*character < 0x20) {
                if (fprintf(output, "\\u%04x", *character) < 0) {
                    return false;
                }
            } else if (fputc(*character, output) == EOF) {
                return false;
            }
            break;
        }
    }

    return fputc('"', output) != EOF;
}

static bool write_packet(
    FILE *output,
    const char *body,
    size_t length
)
{
    if (fprintf(output, "Content-Length: %zu\r\n\r\n", length) < 0
        || fwrite(body, 1, length, output) != length
        || fflush(output) == EOF) {
        return false;
    }

    return true;
}

static bool send_response(
    FuseLspServer *server,
    FuseJsonValue id,
    const char *result
)
{
    char *contents = NULL;
    size_t length = 0;
    FILE *body = open_memstream(&contents, &length);

    if (body == NULL) {
        return false;
    }

    fputs("{\"jsonrpc\":\"2.0\",\"id\":", body);

    if (id.data != NULL && id.length > 0) {
        fwrite(id.data + id.offset, 1, id.length, body);
    } else {
        fputs("null", body);
    }

    fprintf(body, ",\"result\":%s}", result);

    if (fclose(body) == EOF) {
        free(contents);
        return false;
    }

    bool sent = write_packet(server->output, contents, length);
    free(contents);
    return sent;
}

static bool send_error(
    FuseLspServer *server,
    FuseJsonValue id,
    int code,
    const char *message
)
{
    FILE *body = NULL;
    char *contents = NULL;
    size_t length = 0;
    body = open_memstream(&contents, &length);

    if (body == NULL) {
        return false;
    }

    fputs("{\"jsonrpc\":\"2.0\",\"id\":", body);

    if (id.data != NULL && id.length > 0) {
        fwrite(id.data + id.offset, 1, id.length, body);
    } else {
        fputs("null", body);
    }

    fprintf(body, ",\"error\":{\"code\":%d,\"message\":", code);
    write_json_string(body, message);
    fputs("}}", body);

    if (fclose(body) == EOF) {
        free(contents);
        return false;
    }

    bool sent = write_packet(server->output, contents, length);
    free(contents);
    return sent;
}

static bool send_notification(
    FuseLspServer *server,
    const char *method,
    const char *params
)
{
    FILE *body = NULL;
    char *contents = NULL;
    size_t length = 0;
    body = open_memstream(&contents, &length);

    if (body == NULL) {
        return false;
    }

    fputs("{\"jsonrpc\":\"2.0\",\"method\":", body);
    write_json_string(body, method);
    fprintf(body, ",\"params\":%s}", params);

    if (fclose(body) == EOF) {
        free(contents);
        return false;
    }

    bool sent = write_packet(server->output, contents, length);
    free(contents);
    return sent;
}

typedef enum {
    PACKET_ERROR,
    PACKET_END,
    PACKET_READY,
} PacketStatus;

static PacketStatus read_packet(
    FILE *input,
    char **out_body,
    size_t *out_length
)
{
    char *line = NULL;
    size_t capacity = 0;
    size_t content_length = 0;
    bool found_length = false;
    bool started = false;

    for (;;) {
        ssize_t length = getline(&line, &capacity, input);

        if (length < 0) {
            free(line);
            return !started && feof(input) && !ferror(input)
                ? PACKET_END : PACKET_ERROR;
        }

        started = true;
        while (length > 0
            && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }

        if (length == 0) {
            break;
        }

        if (strncmp(line, "Content-Length:", 15) == 0) {
            const char *number = line + 15;

            while (*number == ' ' || *number == '\t') {
                number += 1;
            }

            if (*number < '0' || *number > '9' || found_length) {
                free(line);
                return PACKET_ERROR;
            }

            char *end = NULL;
            errno = 0;
            unsigned long long value = strtoull(number, &end, 10);

            while (*end == ' ' || *end == '\t') {
                end += 1;
            }

            if (errno != 0 || *end != '\0' || value > SIZE_MAX - 1) {
                free(line);
                return PACKET_ERROR;
            }

            content_length = (size_t)value;
            found_length = true;
        }
    }

    free(line);

    if (!found_length || content_length > SIZE_MAX - 1) {
        return PACKET_ERROR;
    }

    char *body = malloc(content_length + 1);

    if (body == NULL) {
        return PACKET_ERROR;
    }

    size_t read_length = 0;

    while (read_length < content_length) {
        size_t count = fread(
            body + read_length,
            1,
            content_length - read_length,
            input
        );

        if (count == 0) {
            free(body);
            return PACKET_ERROR;
        }

        read_length += count;
    }

    body[content_length] = '\0';
    *out_body = body;
    *out_length = content_length;
    return PACKET_READY;
}

static FuseLspPosition position_at(
    const char *source,
    size_t length,
    size_t offset
)
{
    FuseLspPosition position = {0};

    if (offset > length) {
        offset = length;
    }

    for (size_t index = 0; index < offset;) {
        unsigned char character = (unsigned char)source[index];

        if (character == '\n') {
            position.line += 1;
            position.character = 0;
            index += 1;
            continue;
        }

        size_t width = 1;
        uint32_t codepoint = character;

        if (character >= 0xc2 && character <= 0xdf && index + 1 < offset) {
            width = 2;
            codepoint = ((uint32_t)(character & 0x1f) << 6)
                | ((unsigned char)source[index + 1] & 0x3f);
        } else if (character >= 0xe0 && character <= 0xef
            && index + 2 < offset) {
            width = 3;
            codepoint = ((uint32_t)(character & 0x0f) << 12)
                | (((unsigned char)source[index + 1] & 0x3f) << 6)
                | ((unsigned char)source[index + 2] & 0x3f);
        } else if (character >= 0xf0 && character <= 0xf4
            && index + 3 < offset) {
            width = 4;
            codepoint = ((uint32_t)(character & 0x07) << 18)
                | (((unsigned char)source[index + 1] & 0x3f) << 12)
                | (((unsigned char)source[index + 2] & 0x3f) << 6)
                | ((unsigned char)source[index + 3] & 0x3f);
        }

        position.character += codepoint > 0xffff ? 2 : 1;
        index += width;
    }

    return position;
}

static size_t utf8_codepoint_at(
    const char *source,
    size_t length,
    size_t offset,
    uint32_t *out_codepoint
)
{
    unsigned char character = (unsigned char)source[offset];
    size_t width = 1;
    uint32_t codepoint = character;

    if (character >= 0xc2 && character <= 0xdf
        && offset + 1 < length
        && ((unsigned char)source[offset + 1] & 0xc0) == 0x80) {
        width = 2;
        codepoint = ((uint32_t)(character & 0x1f) << 6)
            | ((unsigned char)source[offset + 1] & 0x3f);
    } else if (character >= 0xe0 && character <= 0xef
        && offset + 2 < length
        && ((unsigned char)source[offset + 1] & 0xc0) == 0x80
        && ((unsigned char)source[offset + 2] & 0xc0) == 0x80) {
        width = 3;
        codepoint = ((uint32_t)(character & 0x0f) << 12)
            | (((unsigned char)source[offset + 1] & 0x3f) << 6)
            | ((unsigned char)source[offset + 2] & 0x3f);
    } else if (character >= 0xf0 && character <= 0xf4
        && offset + 3 < length
        && ((unsigned char)source[offset + 1] & 0xc0) == 0x80
        && ((unsigned char)source[offset + 2] & 0xc0) == 0x80
        && ((unsigned char)source[offset + 3] & 0xc0) == 0x80) {
        width = 4;
        codepoint = ((uint32_t)(character & 0x07) << 18)
            | (((unsigned char)source[offset + 1] & 0x3f) << 12)
            | (((unsigned char)source[offset + 2] & 0x3f) << 6)
            | ((unsigned char)source[offset + 3] & 0x3f);
    }

    *out_codepoint = codepoint;
    return width;
}

static size_t offset_at(
    const char *source,
    size_t length,
    FuseLspPosition position
)
{
    size_t offset = 0;
    size_t line = 0;

    while (offset < length && line < position.line) {
        if (source[offset] == '\n') {
            line += 1;
        }

        offset += 1;
    }

    if (line != position.line) {
        return length;
    }

    size_t character = 0;

    while (offset < length
        && source[offset] != '\n'
        && character < position.character) {
        uint32_t codepoint;
        size_t width = utf8_codepoint_at(
            source,
            length,
            offset,
            &codepoint
        );
        size_t units = codepoint > 0xffff ? 2 : 1;

        if (character + units > position.character) {
            break;
        }

        character += units;
        offset += width;
    }

    return offset;
}

static bool identifier_at(
    const char *source,
    size_t length,
    size_t offset,
    SourceSpan *out_span
)
{
    Lexer lexer;
    lexer_init(&lexer, source, length);

    for (;;) {
        Token token = lexer_next(&lexer);

        if (token.kind == TOKEN_EOF) {
            return false;
        }

        size_t end = token.span.offset + token.span.length;

        if (token.kind == TOKEN_IDENTIFIER
            && offset >= token.span.offset
            && offset < end) {
            *out_span = token.span;
            return true;
        }

        if (token.span.offset > offset) {
            return false;
        }
    }
}

static void issue_for_span(
    FuseLspIssue *issue,
    SourceSpan span,
    const char *message
)
{
    if (issue->message == NULL) {
        issue->span = span;
        issue->message = message;
    }
}

static bool analyze_document(
    const FuseLspDocument *document,
    FuseLspIssue *out_issue
)
{
    *out_issue = (FuseLspIssue){0};
    size_t length = strlen(document->text);
    Program *syntax = NULL;
    ParseError parse_error;

    if (!parse_program(
            document->text,
            length,
            &syntax,
            &parse_error
        )) {
        issue_for_span(out_issue, parse_error.span, parse_error.message);
        return true;
    }

    Program *lowered = NULL;
    LowerError lower_error;

    if (!lower_program(syntax, &lowered, &lower_error)) {
        issue_for_span(out_issue, lower_error.span, lower_error.message);
        program_destroy(syntax);
        return true;
    }

    SemanticInfo semantic_info = {0};
    SemanticError semantic_error;
    const LanguageEnvironment *environment = document->dialect->environment;

    if (!semantic_analyze(
            lowered,
            document->text,
            environment,
            &semantic_info,
            &semantic_error
        )) {
        issue_for_span(out_issue, semantic_error.span, semantic_error.message);
    } else if (semantic_info.result_type
        != document->dialect->required_result_type) {
        snprintf(
            out_issue->result_message,
            sizeof(out_issue->result_message),
            "program result must be %s",
            value_type_name(document->dialect->required_result_type)
        );
        issue_for_span(out_issue, lowered->result->span, out_issue->result_message);
    }

    semantic_info_destroy(&semantic_info);
    program_destroy(lowered);
    program_destroy(syntax);
    return true;
}

static bool publish_diagnostics(
    FuseLspServer *server,
    const FuseLspDocument *document
)
{
    FuseLspIssue issue;
    analyze_document(document, &issue);
    FILE *params = NULL;
    char *contents = NULL;
    size_t length = 0;
    params = open_memstream(&contents, &length);

    if (params == NULL) {
        return false;
    }

    fputs("{\"uri\":", params);
    write_json_string(params, document->uri);
    fputs(",\"diagnostics\":[", params);

    if (issue.message != NULL) {
        FuseLspPosition start = position_at(
            document->text,
            strlen(document->text),
            issue.span.offset
        );
        size_t end_offset = issue.span.offset;

        if (issue.span.length <= SIZE_MAX - end_offset) {
            end_offset += issue.span.length;
        }

        FuseLspPosition end = position_at(
            document->text,
            strlen(document->text),
            end_offset
        );
        fprintf(
            params,
            "{\"range\":{\"start\":{\"line\":%zu,\"character\":%zu},"
            "\"end\":{\"line\":%zu,\"character\":%zu}},"
            "\"severity\":1,\"source\":",
            start.line,
            start.character,
            end.line,
            end.character
        );
        write_json_string(params, document->dialect->language_id);
        fputs(",\"message\":", params);
        write_json_string(params, issue.message);
        fputc('}', params);
    }

    fputs("]}", params);

    if (fclose(params) == EOF) {
        free(contents);
        return false;
    }

    bool sent = send_notification(
        server,
        "textDocument/publishDiagnostics",
        contents
    );
    free(contents);
    return sent;
}

static bool write_signature_detail(
    FILE *output,
    const BuiltinSignature *signature
)
{
    if (fputc('(', output) == EOF) {
        return false;
    }

    for (size_t index = 0;
         index < signature->parameter_count;
         index += 1) {
        if (index > 0 && fputs(", ", output) == EOF) {
            return false;
        }

        if (fputs(
                value_type_name(signature->parameters[index]),
                output
            ) == EOF) {
            return false;
        }
    }

    return fprintf(
        output,
        ") -> %s",
        value_type_name(signature->result)
    ) >= 0;
}

static bool write_completion_items(
    FILE *output,
    const LanguageEnvironment *environment,
    bool has_receiver,
    ValueType receiver_type
)
{
    bool first = true;

    for (size_t index = 0;
         index < environment->builtin_count;
         index += 1) {
        const BuiltinSignature *signature = &environment->builtins[index];
        bool seen = false;

        for (size_t previous = 0; previous < index; previous += 1) {
            if (strcmp(
                    environment->builtins[previous].name,
                    signature->name
                ) == 0) {
                seen = true;
                break;
            }
        }

        if (seen) {
            continue;
        }

        if (has_receiver) {
            bool matches_receiver = false;

            for (size_t overload = index;
                 overload < environment->builtin_count;
                 overload += 1) {
                const BuiltinSignature *candidate =
                    &environment->builtins[overload];

                if (strcmp(candidate->name, signature->name) == 0
                    && candidate->parameter_count > 0
                    && candidate->parameters[0] == receiver_type) {
                    matches_receiver = true;
                    break;
                }
            }

            if (!matches_receiver) {
                continue;
            }
        }

        if (!first && fputc(',', output) == EOF) {
            return false;
        }

        first = false;
        fputs("{\"label\":", output);
        write_json_string(output, signature->name);
        fputs(",\"kind\":3,\"detail\":\"", output);
        bool first_signature = true;

        for (size_t overload = index;
             overload < environment->builtin_count;
             overload += 1) {
            const BuiltinSignature *candidate =
                &environment->builtins[overload];

            if (strcmp(candidate->name, signature->name) != 0) {
                continue;
            }

            if (has_receiver
                && (candidate->parameter_count == 0
                    || candidate->parameters[0] != receiver_type)) {
                continue;
            }

            if (!first_signature && fputs("; ", output) == EOF) {
                return false;
            }

            if (!write_signature_detail(output, candidate)) {
                return false;
            }

            first_signature = false;
        }

        fputs("\"}", output);
    }

    if (has_receiver) {
        return true;
    }

    for (size_t index = 0;
         index < environment->global_count;
         index += 1) {
        const GlobalSignature *global = &environment->globals[index];

        if (!first && fputc(',', output) == EOF) {
            return false;
        }

        first = false;
        fputs("{\"label\":", output);
        write_json_string(output, global->name);
        fprintf(
            output,
            ",\"kind\":6,\"detail\":\"%s\"}",
            value_type_name(global->type)
        );
    }

    return true;
}

static const char *find_documentation(
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

static bool identifier_character(char character)
{
    return (character >= 'a' && character <= 'z')
        || (character >= 'A' && character <= 'Z')
        || (character >= '0' && character <= '9')
        || character == '_';
}

static bool completion_dot_offset(
    const char *source,
    size_t length,
    size_t offset,
    size_t *out_dot_offset
)
{
    if (offset > length) {
        offset = length;
    }

    size_t cursor = offset;

    while (cursor > 0
        && identifier_character(source[cursor - 1])) {
        cursor -= 1;
    }

    if (cursor > 0 && source[cursor - 1] == '.') {
        size_t dot_offset = cursor - 1;

        if (dot_offset > 0
            && dot_offset + 1 < length
            && source[dot_offset - 1] >= '0'
            && source[dot_offset - 1] <= '9'
            && source[dot_offset + 1] >= '0'
            && source[dot_offset + 1] <= '9') {
            return false;
        }

        *out_dot_offset = dot_offset;
        return true;
    }

    if (offset > 0 && source[offset - 1] == '.') {
        *out_dot_offset = offset - 1;
        return true;
    }

    return false;
}

static bool infer_completion_receiver_type(
    const FuseLspDocument *document,
    FuseLspPosition position,
    ValueType *out_type
)
{
    const char *source = document->text;
    size_t source_length = strlen(source);
    size_t cursor_offset = offset_at(source, source_length, position);
    size_t dot_offset;

    if (!completion_dot_offset(
            source,
            source_length,
            cursor_offset,
            &dot_offset
        )) {
        return false;
    }

    size_t prefix_length = dot_offset;

    if (prefix_length == 0) {
        return false;
    }

    char *prefix = malloc(prefix_length + 1);

    if (prefix == NULL) {
        return false;
    }

    memcpy(prefix, source, prefix_length);
    prefix[prefix_length] = '\0';

    Program *syntax = NULL;
    ParseError parse_error;
    bool appended_result = false;

    if (!parse_program(
            prefix,
            prefix_length,
            &syntax,
            &parse_error
        )) {
        const char suffix[] = ";\n0.0";
        size_t suffix_length = sizeof(suffix) - 1;
        char *completed = malloc(prefix_length + suffix_length + 1);

        if (completed == NULL) {
            free(prefix);
            return false;
        }

        memcpy(completed, prefix, prefix_length);
        memcpy(completed + prefix_length, suffix, suffix_length + 1);
        free(prefix);
        prefix = completed;
        prefix_length += suffix_length;
        appended_result = true;

        if (!parse_program(
                prefix,
                prefix_length,
                &syntax,
                &parse_error
            )) {
            free(prefix);
            return false;
        }
    }

    Program *lowered = NULL;
    LowerError lower_error;

    if (!lower_program(syntax, &lowered, &lower_error)) {
        program_destroy(syntax);
        free(prefix);
        return false;
    }

    const LanguageEnvironment *environment = document->dialect->environment;
    SemanticInfo semantic_info = {0};
    SemanticError semantic_error;
    bool analyzed = semantic_analyze(
        lowered,
        prefix,
        environment,
        &semantic_info,
        &semantic_error
    );

    if (analyzed) {
        if (appended_result && lowered->binding_count > 0) {
            *out_type = semantic_info.binding_types[
                lowered->binding_count - 1
            ];
        } else {
            *out_type = semantic_info.result_type;
        }
    }

    semantic_info_destroy(&semantic_info);
    program_destroy(lowered);
    program_destroy(syntax);
    free(prefix);
    return analyzed;
}

static bool write_hover_markdown(
    FILE *output,
    const LanguageEnvironment *environment,
    const char *name,
    const char *language_name,
    bool *out_found
)
{
    bool found = false;

    *out_found = false;

    if (fprintf(output, "```%s\n", language_name) < 0) {
        return false;
    }

    for (size_t index = 0;
         index < environment->builtin_count;
         index += 1) {
        const BuiltinSignature *signature = &environment->builtins[index];

        if (strcmp(signature->name, name) != 0) {
            continue;
        }

        if (found && fputc('\n', output) == EOF) {
            return false;
        }

        if (fputs(signature->name, output) == EOF
            || !write_signature_detail(output, signature)) {
            return false;
        }

        found = true;
    }

    for (size_t index = 0;
         index < environment->global_count;
         index += 1) {
        const GlobalSignature *global = &environment->globals[index];

        if (strcmp(global->name, name) != 0) {
            continue;
        }

        if (found && fputc('\n', output) == EOF) {
            return false;
        }

        if (fprintf(
                output,
                "%s: %s",
                global->name,
                value_type_name(global->type)
            ) < 0) {
            return false;
        }

        found = true;
    }

    if (fputs("\n```", output) == EOF) {
        return false;
    }

    const char *documentation = find_documentation(environment, name);

    if (documentation != NULL
        && (fputs("\n\n", output) == EOF
            || fputs(documentation, output) == EOF)) {
        return false;
    }

    *out_found = found;
    return true;
}

static bool send_hover(
    FuseLspServer *server,
    FuseJsonValue id,
    const FuseLspDocument *document,
    FuseLspPosition position
)
{
    size_t source_length = strlen(document->text);
    size_t offset = offset_at(document->text, source_length, position);
    SourceSpan span;

    if (!identifier_at(document->text, source_length, offset, &span)) {
        return send_response(server, id, "null");
    }

    char *name = malloc(span.length + 1);

    if (name == NULL) {
        return false;
    }

    memcpy(name, document->text + span.offset, span.length);
    name[span.length] = '\0';

    const LanguageEnvironment *environment = document->dialect->environment;
    char *markdown = NULL;
    size_t markdown_length = 0;
    FILE *markdown_output = open_memstream(&markdown, &markdown_length);
    bool found = false;
    bool built = markdown_output != NULL
        && write_hover_markdown(
            markdown_output,
            environment,
            name,
            document->dialect->language_id,
            &found
        );

    if (markdown_output != NULL && fclose(markdown_output) == EOF) {
        built = false;
    }

    free(name);

    if (!built || markdown == NULL) {
        free(markdown);

        return false;
    }

    if (!found) {
        free(markdown);
        return send_response(server, id, "null");
    }

    FILE *result = NULL;
    char *contents = NULL;
    size_t contents_length = 0;
    result = open_memstream(&contents, &contents_length);

    if (result == NULL) {
        free(markdown);
        return false;
    }

    fputs("{\"contents\":{\"kind\":\"markdown\",\"value\":", result);
    write_json_string(result, markdown);
    fputs("}}", result);

    if (fclose(result) == EOF) {
        free(contents);
        free(markdown);
        return false;
    }

    bool sent = send_response(server, id, contents);
    free(contents);
    free(markdown);
    return sent;
}

static bool send_completion(
    FuseLspServer *server,
    FuseJsonValue id,
    const FuseLspDocument *document,
    FuseLspPosition position
)
{
    size_t source_length = strlen(document->text);
    size_t offset = offset_at(document->text, source_length, position);
    size_t line_start = offset;

    while (line_start > 0
        && document->text[line_start - 1] != '\n'
        && document->text[line_start - 1] != '\r') {
        line_start -= 1;
    }

    for (size_t index = line_start; index + 1 < offset; index += 1) {
        if (document->text[index] == '/' && document->text[index + 1] == '/') {
            return send_response(server, id, "{\"isIncomplete\":false,\"items\":[]}");
        }
    }

    FILE *result = NULL;
    char *contents = NULL;
    size_t length = 0;
    result = open_memstream(&contents, &length);

    if (result == NULL) {
        return false;
    }

    ValueType receiver_type = VALUE_TYPE_INVALID;
    bool has_receiver = infer_completion_receiver_type(
        document,
        position,
        &receiver_type
    );

    fputs("{\"isIncomplete\":false,\"items\":[", result);
    bool built = write_completion_items(
        result,
        document->dialect->environment,
        has_receiver,
        receiver_type
    );
    fputs("]}", result);

    if (fclose(result) == EOF) {
        built = false;
    }

    bool sent = built && send_response(server, id, contents);
    free(contents);
    return sent;
}

static bool update_document(
    FuseLspServer *server,
    const char *uri,
    const char *text,
    int64_t version
)
{
    FuseLspDocument *document = find_document(server, uri);

    if (document == NULL) {
        if (server->document_count == server->document_capacity) {
            size_t capacity = server->document_capacity == 0
                ? 8
                : server->document_capacity * 2;
            FuseLspDocument *documents = realloc(
                server->documents,
                capacity * sizeof(*documents)
            );

            if (documents == NULL) {
                return false;
            }

            server->documents = documents;
            server->document_capacity = capacity;
        }

        document = &server->documents[server->document_count++];
        *document = (FuseLspDocument){0};
        document->uri = strdup(uri);

        if (document->uri == NULL) {
            server->document_count -= 1;
            return false;
        }
    }

    char *copy = strdup(text);

    if (copy == NULL || document->uri == NULL) {
        free(copy);
        return false;
    }

    free(document->text);
    document->text = copy;
    document->dialect = server->dialect;
    document->version = version;
    return true;
}

static void remove_document(
    FuseLspServer *server,
    const char *uri
)
{
    for (size_t index = 0; index < server->document_count; index += 1) {
        if (strcmp(server->documents[index].uri, uri) != 0) {
            continue;
        }

        document_destroy(&server->documents[index]);
        memmove(
            &server->documents[index],
            &server->documents[index + 1],
            (server->document_count - index - 1)
                * sizeof(*server->documents)
        );
        server->document_count -= 1;
        return;
    }
}

static bool handle_message(
    FuseLspServer *server,
    const char *body,
    size_t length
)
{
    FuseJsonValue root;

    if (!fuse_json_parse(body, length, &root)
        || fuse_json_type(root) != FUSE_JSON_OBJECT) {
        if (server->log != NULL) {
            fprintf(server->log, "%s: invalid JSON message\n", server->dialect->language_id);
        }
        return true;
    }

    FuseJsonValue id = {0};
    char *method = NULL;
    bool has_id = fuse_json_object_get(root, "id", &id);

    if (!json_get_string(root, "method", &method)) {
        return true;
    }

    FuseJsonValue params = {0};
    fuse_json_object_get(root, "params", &params);

    if (server->shutdown_requested && strcmp(method, "exit") != 0) {
        bool handled = !has_id
            || send_error(server, id, -32600, "server is shutting down");
        free(method);
        return handled;
    }

    if (strcmp(method, "initialize") == 0) {
        const char prefix[] =
            "{\"capabilities\":{"
            "\"textDocumentSync\":1,\"hoverProvider\":true,"
            "\"completionProvider\":{\"resolveProvider\":false,"
                "\"triggerCharacters\":["
                "\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\","
                "\"h\",\"i\",\"j\",\"k\",\"l\",\"m\",\"n\","
                "\"o\",\"p\",\"q\",\"r\",\"s\",\"t\",\"u\","
                "\"v\",\"w\",\"x\",\"y\",\"z\","
                "\"A\",\"B\",\"C\",\"D\",\"E\",\"F\",\"G\","
                "\"H\",\"I\",\"J\",\"K\",\"L\",\"M\",\"N\","
                "\"O\",\"P\",\"Q\",\"R\",\"S\",\"T\",\"U\","
                "\"V\",\"W\",\"X\",\"Y\",\"Z\",\"_\",\".\"]}},"
            "\"serverInfo\":{\"name\":";
        char *contents = NULL;
        size_t contents_length = 0;
        FILE *result = open_memstream(&contents, &contents_length);

        if (result == NULL) {
            free(method);
            return false;
        }

        fputs(prefix, result);
        write_json_string(result, server->dialect->language_id);
        fputs("}}", result);
        bool built = !ferror(result);

        if (fclose(result) == EOF) {
            built = false;
        }

        bool handled = built && has_id && send_response(server, id, contents);
        free(contents);
        free(method);
        return handled;
    }

    if (strcmp(method, "initialized") == 0
        || strcmp(method, "$/cancelRequest") == 0
        || strcmp(method, "workspace/didChangeConfiguration") == 0) {
        free(method);
        return true;
    }

    if (strcmp(method, "shutdown") == 0) {
        server->shutdown_requested = true;
        bool handled = !has_id || send_response(server, id, "null");
        free(method);
        return handled;
    }

    if (strcmp(method, "exit") == 0) {
        server->exit_requested = true;
        free(method);
        return true;
    }

    if (strcmp(method, "textDocument/didOpen") == 0
        || strcmp(method, "textDocument/didChange") == 0) {
        FuseJsonValue text_document;
        char *uri = NULL;
        char *text = NULL;
        bool parsed = json_get_object(params, "textDocument", &text_document)
            && json_get_string(text_document, "uri", &uri);

        if (strcmp(method, "textDocument/didOpen") == 0) {
            parsed = parsed
                && json_get_string(text_document, "text", &text);
        } else {
            FuseJsonValue changes;
            FuseJsonValue first_change;
            parsed = parsed
                && json_get_array(params, "contentChanges", &changes)
                && fuse_json_array_first(changes, &first_change)
                && json_get_string(first_change, "text", &text);
        }

        if (parsed) {
            FuseJsonValue version_value;
            int64_t version = 0;

            if (fuse_json_object_get(
                    text_document,
                    "version",
                    &version_value
                )) {
                char *version_text = NULL;

                if (fuse_json_string_copy(version_value, &version_text)) {
                    version = strtoll(version_text, NULL, 10);
                    free(version_text);
                }
            }

            FuseLspDocument *document = find_document(server, uri);
            parsed = update_document(
                server,
                uri,
                text,
                version
            );
            document = find_document(server, uri);

            if (parsed && document != NULL) {
                parsed = publish_diagnostics(server, document);
            }
        }

        free(uri);
        free(text);
        free(method);
        return parsed;
    }

    if (strcmp(method, "textDocument/didClose") == 0) {
        FuseJsonValue text_document;
        char *uri = NULL;
        bool parsed = json_get_object(params, "textDocument", &text_document)
            && json_get_string(text_document, "uri", &uri);

        if (parsed) {
            remove_document(server, uri);
            FILE *notification = NULL;
            char *contents = NULL;
            size_t notification_length = 0;
            notification = open_memstream(&contents, &notification_length);

            if (notification == NULL) {
                parsed = false;
            } else {
                fputs("{\"uri\":", notification);
                write_json_string(notification, uri);
                fputs(",\"diagnostics\":[]}", notification);
                parsed = fclose(notification) != EOF
                    && send_notification(
                        server,
                        "textDocument/publishDiagnostics",
                        contents
                    );
                free(contents);
            }
        }

        free(uri);
        free(method);
        return parsed;
    }

    if (strcmp(method, "textDocument/hover") == 0) {
        FuseJsonValue text_document;
        char *uri = NULL;
        FuseLspPosition position;
        bool parsed = json_get_object(params, "textDocument", &text_document)
            && json_get_string(text_document, "uri", &uri)
            && json_get_position(params, &position);
        FuseLspDocument *document = parsed
            ? find_document(server, uri)
            : NULL;

        if (parsed && document != NULL && has_id) {
            parsed = send_hover(server, id, document, position);
        } else if (parsed && has_id) {
            parsed = send_response(server, id, "null");
        }

        free(uri);
        free(method);
        return parsed;
    }

    if (strcmp(method, "textDocument/completion") == 0) {
        FuseJsonValue text_document;
        char *uri = NULL;
        FuseLspPosition position;
        bool parsed = json_get_object(params, "textDocument", &text_document)
            && json_get_string(text_document, "uri", &uri)
            && json_get_position(params, &position);
        FuseLspDocument *document = parsed
            ? find_document(server, uri)
            : NULL;

        if (parsed && document != NULL && has_id) {
            parsed = send_completion(server, id, document, position);
        } else if (parsed && has_id) {
            parsed = send_response(
                server,
                id,
                "{\"isIncomplete\":false,\"items\":[]}"
            );
        }

        free(uri);
        free(method);
        return parsed;
    }

    bool handled = !has_id
        || send_error(server, id, -32601, "method not found");
    free(method);
    return handled;
}

static void server_destroy(FuseLspServer *server)
{
    for (size_t index = 0; index < server->document_count; index += 1) {
        document_destroy(&server->documents[index]);
    }

    free(server->documents);
}

static bool valid_type(ValueType type)
{
    return type >= VALUE_TYPE_FLOAT && type <= VALUE_TYPE_MESH;
}

static bool valid_dialect(const FuseDialect *dialect)
{
    if (dialect == NULL || dialect->language_id == NULL
        || dialect->language_id[0] == '\0' || dialect->environment == NULL
        || !valid_type(dialect->required_result_type)) {
        return false;
    }

    const LanguageEnvironment *environment = dialect->environment;

    if ((environment->builtin_count > 0 && environment->builtins == NULL)
        || (environment->global_count > 0 && environment->globals == NULL)
        || (environment->documentation_count > 0 && environment->documentation == NULL)
        || (environment->reserved_count > 0 && environment->reserved_names == NULL)) {
        return false;
    }

    for (size_t i = 0; i < environment->builtin_count; i += 1) {
        const BuiltinSignature *signature = &environment->builtins[i];

        if (signature->name == NULL || signature->name[0] == '\0'
            || !valid_type(signature->result)
            || signature->parameter_count > BUILTIN_MAX_PARAMETERS) {
            return false;
        }

        for (size_t parameter = 0; parameter < signature->parameter_count; parameter += 1) {
            if (!valid_type(signature->parameters[parameter])) {
                return false;
            }
        }
    }

    for (size_t i = 0; i < environment->global_count; i += 1) {
        if (environment->globals[i].name == NULL
            || !valid_type(environment->globals[i].type)) {
            return false;
        }
    }

    for (size_t i = 0; i < environment->documentation_count; i += 1) {
        if (environment->documentation[i].name == NULL
            || environment->documentation[i].documentation == NULL) {
            return false;
        }
    }

    for (size_t i = 0; i < environment->reserved_count; i += 1) {
        if (environment->reserved_names[i] == NULL) {
            return false;
        }
    }

    return true;
}

int fuse_lsp_run(
    const FuseDialect *dialect,
    FILE *input,
    FILE *output,
    FILE *log
)
{
    if (!valid_dialect(dialect) || input == NULL || output == NULL) {
        return 1;
    }

    FuseLspServer server = {
        .dialect = dialect,
        .output = output,
        .log = log,
    };
    int status = 0;

    while (!server.exit_requested) {
        char *body = NULL;
        size_t length = 0;
        PacketStatus packet = read_packet(input, &body, &length);

        if (packet != PACKET_READY) {
            status = packet == PACKET_END ? 0 : 1;
            break;
        }

        bool handled = handle_message(&server, body, length);
        free(body);

        if (!handled) {
            status = 1;
            break;
        }
    }

    if (server.exit_requested && !server.shutdown_requested) {
        status = 1;
    }

    server_destroy(&server);
    return status;
}
