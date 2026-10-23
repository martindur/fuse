CC := cc
AR := ar

BUILD_DIR ?= build
OBJECT_DIR := $(BUILD_DIR)/obj
CFLAGS ?= -O2 -g
WARNINGS := -std=c17 -Wall -Wextra -Wpedantic -Wshadow
SANITIZERS ?=

SOURCES := \
	src/lexer.c src/ast.c src/ast_dump.c src/parser.c src/lower.c \
	src/semantic.c src/help.c src/preview.c src/lsp/lsp.c src/lsp/json.c
OBJECTS := $(SOURCES:%.c=$(OBJECT_DIR)/%.o)
LIBRARY := $(BUILD_DIR)/libfuse-lang.a
TEST_NAMES := lexer parser ast_dump lower preview semantic json lsp
TEST_OBJECTS := $(TEST_NAMES:%=$(OBJECT_DIR)/tests/%_test.o)
TEST_TARGETS := $(TEST_NAMES:%=$(BUILD_DIR)/%_test)
TEST_DIALECTS := $(OBJECT_DIR)/tests/dialects.o
GRAMMAR := tree-sitter-fuse
PARSER := $(BUILD_DIR)/tree-sitter/fuse.so

PREFIX ?= /usr/local
INCLUDEDIR ?= $(PREFIX)/include
LIBDIR ?= $(PREFIX)/lib
DATADIR ?= $(PREFIX)/share/fuse
INSTALL ?= install

.PHONY: all test clean install tree-sitter-generate tree-sitter-test tree-sitter-parser install-grammar
.SECONDARY: $(TEST_OBJECTS)

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(OBJECT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -Iinclude -Isrc/lsp $(CFLAGS) $(WARNINGS) $(SANITIZERS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/semantic_test $(BUILD_DIR)/lsp_test: $(TEST_DIALECTS)

$(BUILD_DIR)/%_test: $(OBJECT_DIR)/tests/%_test.o $(LIBRARY)
	$(CC) $(LDFLAGS) $(SANITIZERS) -o $@ $(filter %.o,$^) $(LIBRARY) $(LDLIBS)

test: $(TEST_TARGETS)
	@set -e; for test in $(TEST_TARGETS); do "$$test"; done

tree-sitter-generate:
	cd $(GRAMMAR) && tree-sitter generate

tree-sitter-test:
	tree-sitter test --grammar-path $(GRAMMAR)

tree-sitter-parser: $(PARSER)

$(PARSER): $(GRAMMAR)/src/parser.c $(GRAMMAR)/src/tree_sitter/parser.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -shared -fPIC -I$(GRAMMAR)/src $< -o $@

install: all
	$(INSTALL) -d "$(DESTDIR)$(LIBDIR)" "$(DESTDIR)$(INCLUDEDIR)/fuse"
	$(INSTALL) -m 644 $(LIBRARY) "$(DESTDIR)$(LIBDIR)/"
	$(INSTALL) -m 644 include/fuse/*.h "$(DESTDIR)$(INCLUDEDIR)/fuse/"

install-grammar: tree-sitter-parser
	$(INSTALL) -d "$(DESTDIR)$(DATADIR)/tree-sitter-fuse/parser" "$(DESTDIR)$(DATADIR)/tree-sitter-fuse/queries/fuse"
	$(INSTALL) -m 755 $(PARSER) "$(DESTDIR)$(DATADIR)/tree-sitter-fuse/parser/fuse.so"
	$(INSTALL) -m 644 $(GRAMMAR)/queries/fuse/*.scm "$(DESTDIR)$(DATADIR)/tree-sitter-fuse/queries/fuse/"

clean:
	$(RM) -r $(BUILD_DIR)

-include $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d) $(TEST_DIALECTS:.o=.d)
