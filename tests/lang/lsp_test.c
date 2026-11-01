#define _POSIX_C_SOURCE 200809L

#include "dialects.h"
#include "fuse/lsp.h"
#include "json.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char INITIALIZE[] =
    "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}";
static const char SHUTDOWN[] =
    "{\"jsonrpc\":\"2.0\",\"id\":99,\"method\":\"shutdown\"}";
static const char EXIT[] = "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}";

typedef struct {
    int status;
    char *output;
    size_t length;
    bool framed;
} Session;

static bool next_packet(const char **cursor, const char *end, FuseJsonValue *out)
{
    const char *header_end = strstr(*cursor, "\r\n\r\n");
    size_t length = 0;

    if (header_end == NULL
        || sscanf(*cursor, "Content-Length: %zu", &length) != 1) {
        return false;
    }

    const char *body = header_end + 4;

    if ((size_t)(end - body) < length || !fuse_json_parse(body, length, out)) {
        return false;
    }

    *cursor = body + length;
    return true;
}

static Session run_session(
    const FuseDialect *dialect,
    const char *const *messages,
    size_t count
)
{
    Session session = {.status = -1};
    FILE *input = tmpfile();
    FILE *output = open_memstream(&session.output, &session.length);

    if (input != NULL && output != NULL) {
        bool written = true;

        for (size_t i = 0; i < count; i += 1) {
            written = written && fprintf(input, "Content-Length: %zu\r\n\r\n%s",
                strlen(messages[i]), messages[i]) > 0;
        }

        if (written && fflush(input) == 0 && fseek(input, 0, SEEK_SET) == 0) {
            session.status = fuse_lsp_run(&dialect, 1, input, output, NULL);
        }
    }

    if (input != NULL) {
        fclose(input);
    }

    if (output != NULL && fclose(output) == 0) {
        const char *cursor = session.output;
        const char *end = cursor + session.length;
        session.framed = true;

        while (cursor < end) {
            FuseJsonValue packet;

            if (!next_packet(&cursor, end, &packet)) {
                session.framed = false;
                break;
            }
        }
    }

    return session;
}

static bool contains(FuseJsonValue value, const char *text)
{
    size_t length = strlen(text);

    if (value.data == NULL || length > value.length) {
        return false;
    }

    for (size_t i = 0; i <= value.length - length; i += 1) {
        if (memcmp(value.data + value.offset + i, text, length) == 0) {
            return true;
        }
    }

    return false;
}

static bool field_string_is(FuseJsonValue object, const char *field, const char *expected)
{
    FuseJsonValue value;
    char *text = NULL;
    bool matches = fuse_json_object_get(object, field, &value)
        && fuse_json_string_copy(value, &text) && strcmp(text, expected) == 0;
    free(text);
    return matches;
}

static FuseJsonValue result_for(const Session *session, int id)
{
    const char *cursor = session->output;
    const char *end = cursor + session->length;
    char expected[32];
    snprintf(expected, sizeof(expected), "%d", id);

    while (cursor < end) {
        FuseJsonValue packet;
        FuseJsonValue packet_id;
        FuseJsonValue result;

        if (!next_packet(&cursor, end, &packet)) {
            break;
        }

        if (fuse_json_object_get(packet, "id", &packet_id)
            && packet_id.length == strlen(expected)
            && contains(packet_id, expected)
            && fuse_json_object_get(packet, "result", &result)) {
            return result;
        }
    }

    return (FuseJsonValue){0};
}

static FuseJsonValue diagnostics_for(const Session *session, size_t index)
{
    const char *cursor = session->output;
    const char *end = cursor + session->length;
    size_t found = 0;

    while (cursor < end) {
        FuseJsonValue packet;

        if (!next_packet(&cursor, end, &packet)) {
            break;
        }

        if (field_string_is(packet, "method", "textDocument/publishDiagnostics")) {
            if (found++ == index) {
                FuseJsonValue params;
                FuseJsonValue diagnostics;

                if (fuse_json_object_get(packet, "params", &params)
                    && fuse_json_object_get(params, "diagnostics", &diagnostics)) {
                    return diagnostics;
                }
            }
        }
    }

    return (FuseJsonValue){0};
}

static bool check(bool passed, const char *name, const Session *session)
{
    if (!passed) {
        fprintf(stderr, "FAIL %s (status %d):\n%s\n", name, session->status,
            session->output == NULL ? "(no output)" : session->output);
    }

    return passed;
}

static bool test_document_lifecycle(void)
{
    static const char *const MESSAGES[] = {
        INITIALIZE,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{"
            "\"uri\":\"file:///tmp/main.unknown\",\"languageId\":\"test-point\",\"version\":1,"
            "\"text\":\"point.shift(1.0)\"}}}",
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/completion\",\"params\":{"
            "\"textDocument\":{\"uri\":\"file:///tmp/main.unknown\"},"
            "\"position\":{\"line\":0,\"character\":6}}}",
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/hover\",\"params\":{"
            "\"textDocument\":{\"uri\":\"file:///tmp/main.unknown\"},"
            "\"position\":{\"line\":0,\"character\":8}}}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{"
            "\"textDocument\":{\"uri\":\"file:///tmp/main.unknown\",\"version\":2},"
            "\"contentChanges\":[{\"text\":\"point.magnitude().finish()\"}]}}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didClose\",\"params\":{"
            "\"textDocument\":{\"uri\":\"file:///tmp/main.unknown\"}}}",
        "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"textDocument/hover\",\"params\":{"
            "\"textDocument\":{\"uri\":\"file:///tmp/main.unknown\"},"
            "\"position\":{\"line\":0,\"character\":8}}}",
        SHUTDOWN, EXIT,
    };
    Session session = run_session(&TEST_POINT_DIALECT, MESSAGES,
        sizeof(MESSAGES) / sizeof(MESSAGES[0]));
    FuseJsonValue completion = result_for(&session, 2);
    FuseJsonValue hover = result_for(&session, 3);
    bool passed = session.status == 0 && session.framed
        && contains(result_for(&session, 1), "\"name\":\"fuse\"")
        && contains(diagnostics_for(&session, 0), "program result must be vec4")
        && contains(diagnostics_for(&session, 0), "\"source\":\"test-point\"")
        && contains(completion, "\"label\":\"shift\"")
        && contains(completion, "\"detail\":\"(vec2, float) -> vec2\"")
        && !contains(completion, "(float, float) -> float")
        && !contains(completion, "\"label\":\"finish\"")
        && !contains(completion, "\"label\":\"object\"")
        && contains(hover, "shift(vec2, float) -> vec2")
        && contains(hover, "Offsets a test point")
        && contains(hover, "```test-point")
        && contains(diagnostics_for(&session, 1), "[]")
        && contains(diagnostics_for(&session, 2), "[]")
        && fuse_json_type(result_for(&session, 5)) == FUSE_JSON_NULL
        && fuse_json_type(result_for(&session, 99)) == FUSE_JSON_NULL;
    passed = check(passed, "document lifecycle, catalog, and identity", &session);
    free(session.output);
    return passed;
}

static bool test_completion(
    const FuseDialect *dialect,
    const char *source,
    const char *expected,
    const char *excluded
)
{
    char open[1024];
    char completion[512];
    snprintf(open, sizeof(open),
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{"
        "\"uri\":\"untitled:buffer\",\"languageId\":\"%s\",\"version\":1,\"text\":\"%s\"}}}", dialect->language_id, source);
    snprintf(completion, sizeof(completion),
        "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/completion\",\"params\":{"
        "\"textDocument\":{\"uri\":\"untitled:buffer\"},\"position\":{\"line\":0,\"character\":%zu}}}",
        strlen(source));
    const char *messages[] = {INITIALIZE, open, completion, SHUTDOWN, EXIT};
    Session session = run_session(dialect, messages, sizeof(messages) / sizeof(messages[0]));
    FuseJsonValue result = result_for(&session, 2);
    bool passed = check(session.status == 0 && session.framed
        && contains(result, expected) && !contains(result, excluded), source, &session);
    free(session.output);
    return passed;
}

static bool test_object_diagnostics_and_hover(void)
{
    static const char *const MESSAGES[] = {
        INITIALIZE,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{"
            "\"uri\":\"untitled:object\",\"languageId\":\"test-object\",\"version\":1,\"text\":\"object().resize(2.0)\"}}}",
        "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/hover\",\"params\":{"
            "\"textDocument\":{\"uri\":\"untitled:object\"},\"position\":{\"line\":0,\"character\":10}}}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{"
            "\"textDocument\":{\"uri\":\"untitled:object\",\"version\":2},"
            "\"contentChanges\":[{\"text\":\"1.0\"}]}}",
        SHUTDOWN, EXIT,
    };
    Session session = run_session(&TEST_OBJECT_DIALECT, MESSAGES,
        sizeof(MESSAGES) / sizeof(MESSAGES[0]));
    bool passed = check(session.status == 0 && session.framed
        && contains(result_for(&session, 1), "\"name\":\"fuse\"")
        && contains(diagnostics_for(&session, 0), "[]")
        && contains(diagnostics_for(&session, 1), "program result must be mesh")
        && contains(result_for(&session, 3), "resize(mesh, float) -> mesh")
        && contains(result_for(&session, 3), "Resizes the test object."),
        "object dialect diagnostics and hover", &session);
    free(session.output);
    return passed;
}

static bool test_configuration_and_lifecycle_errors(void)
{
    const char *exit_messages[] = {EXIT};
    Session early_exit = run_session(&TEST_OBJECT_DIALECT, exit_messages, 1);
    bool passed = check(early_exit.status != 0, "exit before shutdown", &early_exit);
    free(early_exit.output);
    Session empty = run_session(&TEST_OBJECT_DIALECT, NULL, 0);
    passed = check(empty.status == 0 && empty.framed, "clean EOF", &empty) && passed;
    free(empty.output);

    FuseDialect invalid = TEST_OBJECT_DIALECT;
    invalid.required_result_type = VALUE_TYPE_INVALID;
    Session bad_type = run_session(&invalid, NULL, 0);
    passed = check(bad_type.status != 0, "invalid result type", &bad_type) && passed;
    free(bad_type.output);
    invalid = TEST_OBJECT_DIALECT;
    invalid.environment = NULL;
    Session no_environment = run_session(&invalid, NULL, 0);
    passed = check(no_environment.status != 0, "missing environment", &no_environment) && passed;
    free(no_environment.output);
    LanguageEnvironment broken_environment = {.builtin_count = 1};
    invalid.environment = &broken_environment;
    Session broken = run_session(&invalid, NULL, 0);
    passed = check(broken.status != 0, "missing builtin table", &broken) && passed;
    free(broken.output);
    invalid = TEST_OBJECT_DIALECT;
    invalid.file_extension = NULL;
    Session no_extension = run_session(&invalid, NULL, 0);
    passed = check(no_extension.status != 0, "missing extension", &no_extension) && passed;
    free(no_extension.output);
    passed = fuse_lsp_run(NULL, 0, NULL, NULL, NULL) != 0 && passed;

    FILE *input = tmpfile();
    FILE *output = tmpfile();
    if (input == NULL || output == NULL) {
        passed = false;
    } else {
        const FuseDialect *duplicates[] = {&TEST_OBJECT_DIALECT, &TEST_OBJECT_DIALECT};
        passed = fuse_lsp_run(duplicates, 2, input, output, NULL) != 0 && passed;
        passed = fuse_lsp_run(duplicates, 0, input, output, NULL) != 0 && passed;
        const FuseDialect *missing[] = {NULL};
        passed = fuse_lsp_run(missing, 1, input, output, NULL) != 0 && passed;
    }
    if (input != NULL) {
        fclose(input);
    }
    if (output != NULL) {
        fclose(output);
    }
    return passed;
}

static bool test_transport_errors(void)
{
    static const char *const INVALID[] = {
        "Content-Length: 30\r\n\r\n{}",
        "Content-Length: -1\r\n\r\n",
        "Content-Length: 2oops\r\n\r\n{}",
        "Content-Length: 2\r\nContent-Length: 2\r\n\r\n{}",
        "Content-Type: application/json\r\n\r\n{}",
        "Content-Length: 2\r\n",
    };
    bool passed = true;

    for (size_t i = 0; i < sizeof(INVALID) / sizeof(INVALID[0]); i += 1) {
        FILE *input = fmemopen((void *)INVALID[i], strlen(INVALID[i]), "r");
        FILE *output = tmpfile();

        if (input == NULL || output == NULL
            || fuse_lsp_run((const FuseDialect *[]){&TEST_OBJECT_DIALECT}, 1, input, output, NULL) == 0) {
            fprintf(stderr, "FAIL malformed/truncated packet %zu\n", i);
            passed = false;
        }

        if (input != NULL) {
            fclose(input);
        }
        if (output != NULL) {
            fclose(output);
        }
    }

    FILE *input = tmpfile();
    char buffer[] = "readonly";
    FILE *output = fmemopen(buffer, sizeof(buffer), "r");

    if (input == NULL || output == NULL) {
        passed = false;
    } else {
        fprintf(input, "Content-Length: %zu\r\n\r\n%s", strlen(INITIALIZE), INITIALIZE);
        rewind(input);
        passed = fuse_lsp_run((const FuseDialect *[]){&TEST_POINT_DIALECT}, 1, input, output, NULL) != 0 && passed;
    }

    if (input != NULL) {
        fclose(input);
    }
    if (output != NULL) {
        fclose(output);
    }
    return passed;
}

int main(void)
{
    size_t passed = 0;
    passed += test_document_lifecycle();
    passed += test_completion(&TEST_POINT_DIALECT, "point.", "\"label\":\"shift\"", "\"label\":\"finish\"");
    passed += test_completion(&TEST_POINT_DIALECT, "let p = point.", "\"label\":\"magnitude\"", "\"label\":\"finish\"");
    passed += test_completion(&TEST_OBJECT_DIALECT, "let item = object().", "\"label\":\"resize\"", "\"label\":\"object\"");
    passed += test_completion(&TEST_OBJECT_DIALECT, "object().measure().", "\"items\":[]", "\"label\":\"resize\"");
    passed += test_completion(&TEST_POINT_DIALECT, "", "\"label\":\"point\"", "\"label\":\"resize\"");
    passed += test_completion(&TEST_OBJECT_DIALECT, "", "\"label\":\"object\"", "\"label\":\"point\"");
    passed += test_object_diagnostics_and_hover();
    passed += test_configuration_and_lifecycle_errors();
    passed += test_transport_errors();
    printf("%zu/10 LSP tests passed\n", passed);
    return passed == 10 ? EXIT_SUCCESS : EXIT_FAILURE;
}
