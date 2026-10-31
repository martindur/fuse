CC := cc
AR := ar
PYTHON ?= python3

BUILD_DIR ?= build
OBJECT_DIR := $(BUILD_DIR)/obj
CFLAGS ?= -O2 -g
WARNINGS := -std=c17 -Wall -Wextra -Wpedantic -Wshadow
SANITIZERS ?=
PNG_CFLAGS = $(shell pkg-config --cflags libpng 2>/dev/null)
PNG_LIBS = $(shell pkg-config --libs libpng 2>/dev/null)

LANG_SOURCES := src/lang/lexer.c src/lang/ast.c src/lang/ast_dump.c \
	src/lang/parser.c src/lang/lower.c src/lang/semantic.c src/lang/help.c \
	src/lang/preview.c src/lsp/lsp.c src/lsp/json.c
GL_SOURCES := src/gl/main.c src/gl/glsl_emit.c src/gl/glsl_stdlib.c
TEX_SOURCES := src/tex/main.c src/tex/builtins.c src/tex/evaluate.c \
	src/tex/image.c src/tex/png_write.c src/tex/render.c
MESH_SOURCES := src/mesh/main.c src/mesh/builtins.c src/mesh/evaluate.c \
	src/mesh/mesh.c src/mesh/render.c
LANG_OBJECTS := $(LANG_SOURCES:%.c=$(OBJECT_DIR)/%.o)
GL_OBJECTS := $(GL_SOURCES:%.c=$(OBJECT_DIR)/%.o)
TEX_OBJECTS := $(TEX_SOURCES:%.c=$(OBJECT_DIR)/%.o)
MESH_OBJECTS := $(MESH_SOURCES:%.c=$(OBJECT_DIR)/%.o)
OBJECTS := $(LANG_OBJECTS) $(GL_OBJECTS) $(TEX_OBJECTS) $(MESH_OBJECTS)
LIBRARY := $(BUILD_DIR)/libfuse-lang.a
GL_TARGET := $(BUILD_DIR)/fusegl
TEX_TARGET := $(BUILD_DIR)/fusetex
MESH_TARGET := $(BUILD_DIR)/fusemesh

LANG_TEST_NAMES := lexer parser ast_dump lower preview semantic json lsp
LANG_TEST_TARGETS := $(LANG_TEST_NAMES:%=$(BUILD_DIR)/tests/lang/%_test)
GL_TEST_TARGETS := $(BUILD_DIR)/tests/gl/semantic_test $(BUILD_DIR)/tests/gl/glsl_emit_test
TEX_TEST_TARGETS := $(BUILD_DIR)/tests/tex/evaluate_test
MESH_TEST_TARGETS := $(BUILD_DIR)/tests/mesh/evaluate_test
TEST_TARGETS := $(LANG_TEST_TARGETS) $(GL_TEST_TARGETS) $(TEX_TEST_TARGETS) $(MESH_TEST_TARGETS)
TEST_OBJECTS := $(TEST_TARGETS:$(BUILD_DIR)/%=$(OBJECT_DIR)/%.o)
TEST_DIALECTS := $(OBJECT_DIR)/tests/lang/dialects.o
GRAMMAR := tree-sitter-fuse
PARSER := $(BUILD_DIR)/tree-sitter/fuse.so

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
INCLUDEDIR ?= $(PREFIX)/include
LIBDIR ?= $(PREFIX)/lib
MANDIR ?= $(PREFIX)/share/man
DATADIR ?= $(PREFIX)/share/fuse
INSTALL ?= install

.PHONY: all language fusegl fusetex fusemesh test test-lang test-fusegl test-fusetex test-fusemesh test-install test-shaders \
	clean install install-library install-fusegl install-fusetex install-fusemesh \
	install-grammar install-editor tree-sitter-generate tree-sitter-test tree-sitter-parser
.SECONDARY: $(TEST_OBJECTS)

all: $(GL_TARGET) $(TEX_TARGET) $(MESH_TARGET)
language: $(LIBRARY)
fusegl: $(GL_TARGET)
fusetex: $(TEX_TARGET)
fusemesh: $(MESH_TARGET)

$(LIBRARY): $(LANG_OBJECTS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(OBJECT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -Iinclude -Isrc $(CFLAGS) $(WARNINGS) $(SANITIZERS) -MMD -MP -c $< -o $@

$(TEX_OBJECTS): CPPFLAGS += $(PNG_CFLAGS)
$(OBJECT_DIR)/tests/lang/json_test.o: CPPFLAGS += -Isrc/lsp

$(GL_TARGET): $(GL_OBJECTS) $(LIBRARY)
	$(CC) $(LDFLAGS) $(SANITIZERS) -o $@ $(GL_OBJECTS) $(LIBRARY) $(LDLIBS)

$(TEX_TARGET): $(TEX_OBJECTS) $(LIBRARY)
	$(CC) $(LDFLAGS) $(SANITIZERS) -o $@ $(TEX_OBJECTS) $(LIBRARY) $(PNG_LIBS) -lm $(LDLIBS)

$(MESH_TARGET): $(MESH_OBJECTS) $(LIBRARY)
	$(CC) $(LDFLAGS) $(SANITIZERS) -o $@ $(MESH_OBJECTS) $(LIBRARY) -lm $(LDLIBS)

$(BUILD_DIR)/tests/lang/semantic_test $(BUILD_DIR)/tests/lang/lsp_test: $(TEST_DIALECTS)
$(OBJECT_DIR)/tests/lang/lsp_test.o: CPPFLAGS += -Isrc/lsp
$(GL_TEST_TARGETS): $(filter-out $(OBJECT_DIR)/src/gl/main.o,$(GL_OBJECTS))
$(TEX_TEST_TARGETS): $(filter-out $(OBJECT_DIR)/src/tex/main.o,$(TEX_OBJECTS))
$(MESH_TEST_TARGETS): $(filter-out $(OBJECT_DIR)/src/mesh/main.o,$(MESH_OBJECTS))
$(TEX_TEST_TARGETS): LDLIBS += $(PNG_LIBS) -lm
$(MESH_TEST_TARGETS): LDLIBS += -lm

$(BUILD_DIR)/tests/%_test: $(OBJECT_DIR)/tests/%_test.o $(LIBRARY)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(SANITIZERS) -o $@ $(filter %.o,$^) $(LIBRARY) $(LDLIBS)

test: test-lang test-fusegl test-fusetex test-fusemesh test-install

test-install:
	$(PYTHON) -m tests.install_test

test-shaders: $(GL_TARGET)
	$(PYTHON) -m tests.gl.validate_examples "$(abspath $(GL_TARGET))"

test-lang: $(LANG_TEST_TARGETS)
	@set -e; for test in $(LANG_TEST_TARGETS); do "$$test"; done

test-fusegl: $(GL_TARGET) $(GL_TEST_TARGETS)
	@set -e; for test in $(GL_TEST_TARGETS); do "$$test"; done
	$(PYTHON) -m tests.gl.cli_test "$(abspath $(GL_TARGET))"

test-fusetex: $(TEX_TARGET) $(TEX_TEST_TARGETS)
	@set -e; for test in $(TEX_TEST_TARGETS); do "$$test"; done
	$(PYTHON) -m tests.tex.cli_test "$(abspath $(TEX_TARGET))"

test-fusemesh: $(MESH_TARGET) $(MESH_TEST_TARGETS)
	@set -e; for test in $(MESH_TEST_TARGETS); do "$$test"; done
	$(PYTHON) -m tests.mesh.cli_test "$(abspath $(MESH_TARGET))"

tree-sitter-generate:
	cd $(GRAMMAR) && tree-sitter generate

tree-sitter-test:
	tree-sitter test --grammar-path $(GRAMMAR)

tree-sitter-parser: $(PARSER)

$(PARSER): $(GRAMMAR)/src/parser.c $(GRAMMAR)/src/tree_sitter/parser.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -shared -fPIC -I$(GRAMMAR)/src $< -o $@

install: install-fusegl install-fusetex install-fusemesh

install-fusegl: $(GL_TARGET)
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)" "$(DESTDIR)$(MANDIR)/man1"
	$(INSTALL) -m 755 $(GL_TARGET) "$(DESTDIR)$(BINDIR)/fusegl"
	$(INSTALL) -m 644 docs/man/fusegl.1 "$(DESTDIR)$(MANDIR)/man1/"

install-fusetex: $(TEX_TARGET)
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)" "$(DESTDIR)$(MANDIR)/man1"
	$(INSTALL) -m 755 $(TEX_TARGET) "$(DESTDIR)$(BINDIR)/fusetex"
	$(INSTALL) -m 644 docs/man/fusetex.1 "$(DESTDIR)$(MANDIR)/man1/"

install-fusemesh: $(MESH_TARGET)
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)" "$(DESTDIR)$(MANDIR)/man1"
	$(INSTALL) -m 755 $(MESH_TARGET) "$(DESTDIR)$(BINDIR)/fusemesh"
	$(INSTALL) -m 644 docs/man/fusemesh.1 "$(DESTDIR)$(MANDIR)/man1/"

install-library: $(LIBRARY)
	$(INSTALL) -d "$(DESTDIR)$(LIBDIR)" "$(DESTDIR)$(INCLUDEDIR)/fuse"
	$(INSTALL) -m 644 $(LIBRARY) "$(DESTDIR)$(LIBDIR)/"
	$(INSTALL) -m 644 include/fuse/*.h "$(DESTDIR)$(INCLUDEDIR)/fuse/"

install-grammar: tree-sitter-parser
	$(INSTALL) -d "$(DESTDIR)$(DATADIR)/tree-sitter-fuse/parser" "$(DESTDIR)$(DATADIR)/tree-sitter-fuse/queries/fuse"
	$(INSTALL) -m 755 $(PARSER) "$(DESTDIR)$(DATADIR)/tree-sitter-fuse/parser/fuse.so"
	$(INSTALL) -m 644 $(GRAMMAR)/queries/fuse/*.scm "$(DESTDIR)$(DATADIR)/tree-sitter-fuse/queries/fuse/"

install-editor: install-grammar
	$(INSTALL) -d "$(DESTDIR)$(DATADIR)/nvim/ftdetect" "$(DESTDIR)$(DATADIR)/nvim/lua/fuse" "$(DESTDIR)$(DATADIR)/nvim/plugin"
	$(INSTALL) -m 644 editors/nvim/ftdetect/fuse.lua "$(DESTDIR)$(DATADIR)/nvim/ftdetect/"
	$(INSTALL) -m 644 editors/nvim/lua/fuse/lsp.lua "$(DESTDIR)$(DATADIR)/nvim/lua/fuse/"
	$(INSTALL) -m 644 editors/nvim/plugin/fuse.lua "$(DESTDIR)$(DATADIR)/nvim/plugin/"

clean:
	$(RM) -r $(BUILD_DIR)

-include $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d) $(TEST_DIALECTS:.o=.d)
