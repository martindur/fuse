CC := cc
AR := ar
PYTHON ?= python3

CAPABILITIES ?= gl tex mesh
ENABLED_CAPABILITIES := $(sort $(CAPABILITIES))
ifneq ($(filter-out gl tex mesh,$(ENABLED_CAPABILITIES)),)
$(error Unknown CAPABILITIES: $(filter-out gl tex mesh,$(ENABLED_CAPABILITIES)))
endif
ifeq ($(ENABLED_CAPABILITIES),)
$(error CAPABILITIES must include at least one of gl, tex, mesh)
endif

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
GL_SOURCES := src/gl/command.c src/gl/glsl_emit.c src/gl/glsl_stdlib.c
TEX_SOURCES := src/tex/command.c src/tex/builtins.c src/tex/evaluate.c \
	src/tex/image.c src/tex/png_write.c src/tex/render.c
MESH_SOURCES := src/mesh/command.c src/mesh/builtins.c src/mesh/evaluate.c \
	src/mesh/mesh.c src/mesh/render.c
LANG_OBJECTS := $(LANG_SOURCES:%.c=$(OBJECT_DIR)/%.o)
GL_OBJECTS := $(GL_SOURCES:%.c=$(OBJECT_DIR)/%.o)
TEX_OBJECTS := $(TEX_SOURCES:%.c=$(OBJECT_DIR)/%.o)
MESH_OBJECTS := $(MESH_SOURCES:%.c=$(OBJECT_DIR)/%.o)
gl_OBJECTS := $(GL_OBJECTS)
tex_OBJECTS := $(TEX_OBJECTS)
mesh_OBJECTS := $(MESH_OBJECTS)
SELECTED_OBJECTS := $(foreach capability,$(ENABLED_CAPABILITIES),$($(capability)_OBJECTS))
CLI_OBJECT := $(OBJECT_DIR)/src/cli/main.o
OBJECTS := $(LANG_OBJECTS) $(GL_OBJECTS) $(TEX_OBJECTS) $(MESH_OBJECTS) $(CLI_OBJECT)
LIBRARY := $(BUILD_DIR)/libfuse-lang.a
TARGET := $(BUILD_DIR)/fuse
CAPABILITY_CONFIG := $(BUILD_DIR)/capabilities

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

.PHONY: all language test test-lang test-gl test-tex test-mesh test-cli test-install test-shaders \
	test-release release FORCE \
	clean install install-library install-grammar install-editor \
	tree-sitter-generate tree-sitter-test tree-sitter-parser
.SECONDARY: $(TEST_OBJECTS)

all: $(TARGET)
language: $(LIBRARY)

$(LIBRARY): $(LANG_OBJECTS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(OBJECT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -Iinclude -Isrc $(CFLAGS) $(WARNINGS) $(SANITIZERS) -MMD -MP -c $< -o $@

# Only update the stamp when selection changes, so reused build directories stay correct.
$(CAPABILITY_CONFIG): FORCE
	@mkdir -p $(dir $@)
	@printf '%s\n' '$(ENABLED_CAPABILITIES)' > $@.tmp
	@cmp -s $@.tmp $@ || cp $@.tmp $@
	@$(RM) $@.tmp

$(CLI_OBJECT): $(CAPABILITY_CONFIG)
$(CLI_OBJECT): CPPFLAGS += $(if $(filter gl,$(ENABLED_CAPABILITIES)),-DFUSE_CAP_GL) \
	$(if $(filter tex,$(ENABLED_CAPABILITIES)),-DFUSE_CAP_TEX) \
	$(if $(filter mesh,$(ENABLED_CAPABILITIES)),-DFUSE_CAP_MESH)
$(TEX_OBJECTS): CPPFLAGS += $(PNG_CFLAGS)
$(OBJECT_DIR)/tests/lang/json_test.o: CPPFLAGS += -Isrc/lsp

$(TARGET): $(CLI_OBJECT) $(SELECTED_OBJECTS) $(LIBRARY)
	$(CC) $(LDFLAGS) $(SANITIZERS) -o $@ $(CLI_OBJECT) $(SELECTED_OBJECTS) $(LIBRARY) \
		$(if $(filter tex,$(ENABLED_CAPABILITIES)),$(PNG_LIBS)) \
		$(if $(filter tex mesh,$(ENABLED_CAPABILITIES)),-lm) $(LDLIBS)

$(BUILD_DIR)/tests/lang/semantic_test $(BUILD_DIR)/tests/lang/lsp_test: $(TEST_DIALECTS)
$(OBJECT_DIR)/tests/lang/lsp_test.o: CPPFLAGS += -Isrc/lsp
$(GL_TEST_TARGETS): $(filter-out $(OBJECT_DIR)/src/gl/command.o,$(GL_OBJECTS))
$(TEX_TEST_TARGETS): $(filter-out $(OBJECT_DIR)/src/tex/command.o,$(TEX_OBJECTS))
$(MESH_TEST_TARGETS): $(filter-out $(OBJECT_DIR)/src/mesh/command.o,$(MESH_OBJECTS))
$(TEX_TEST_TARGETS): LDLIBS += $(PNG_LIBS) -lm
$(MESH_TEST_TARGETS): LDLIBS += -lm

$(BUILD_DIR)/tests/%_test: $(OBJECT_DIR)/tests/%_test.o $(LIBRARY)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(SANITIZERS) -o $@ $(filter %.o,$^) $(LIBRARY) $(LDLIBS)

test: test-lang $(ENABLED_CAPABILITIES:%=test-%) test-cli test-release
ifeq ($(ENABLED_CAPABILITIES),gl mesh tex)
test: test-install
endif

test-release:
	$(PYTHON) -m tests.release_test

release: export FUSE_RELEASE_VERSION = $(VERSION)
release:
	$(PYTHON) scripts/release.py open-pr

test-install:
	$(PYTHON) -m tests.install_test

test-cli: $(TARGET)
	$(PYTHON) -m tests.cli_test "$(abspath $(TARGET))" $(ENABLED_CAPABILITIES)

test-shaders: $(TARGET)
	$(PYTHON) -m tests.gl.validate_examples "$(abspath $(TARGET))"

test-lang: $(LANG_TEST_TARGETS)
	@set -e; for test in $(LANG_TEST_TARGETS); do "$$test"; done

test-gl: $(TARGET) $(GL_TEST_TARGETS)
	@set -e; for test in $(GL_TEST_TARGETS); do "$$test"; done
	$(PYTHON) -m tests.gl.cli_test "$(abspath $(TARGET))"

test-tex: $(TARGET) $(TEX_TEST_TARGETS)
	@set -e; for test in $(TEX_TEST_TARGETS); do "$$test"; done
	$(PYTHON) -m tests.tex.cli_test "$(abspath $(TARGET))"

test-mesh: $(TARGET) $(MESH_TEST_TARGETS)
	@set -e; for test in $(MESH_TEST_TARGETS); do "$$test"; done
	$(PYTHON) -m tests.mesh.cli_test "$(abspath $(TARGET))"

tree-sitter-generate:
	cd $(GRAMMAR) && tree-sitter generate

tree-sitter-test:
	tree-sitter test --grammar-path $(GRAMMAR)

tree-sitter-parser: $(PARSER)

$(PARSER): $(GRAMMAR)/src/parser.c $(GRAMMAR)/src/tree_sitter/parser.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -shared -fPIC -I$(GRAMMAR)/src $< -o $@

install: $(TARGET)
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)" "$(DESTDIR)$(MANDIR)/man1"
	$(INSTALL) -m 755 $(TARGET) "$(DESTDIR)$(BINDIR)/fuse"
	$(INSTALL) -m 644 docs/man/fuse.1 "$(DESTDIR)$(MANDIR)/man1/"

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
