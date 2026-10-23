#ifndef FUSE_SOURCE_H
#define FUSE_SOURCE_H

#include <stddef.h>

typedef struct {
  size_t offset;
  size_t length;
} SourceSpan;

typedef struct {
  size_t line;
  size_t column;
} SourcePosition;

static inline SourcePosition source_position_at(
    const char *source,
    size_t length,
    size_t offset
) {
  SourcePosition position = {
    .line = 1,
    .column = 1,
  };

  if (source == NULL) {
    return position;
  }

  if (offset > length) {
    offset = length;
  }

  for (size_t index = 0; index < offset; index += 1) {
    if (source[index] == '\n') {
      position.line += 1;
      position.column = 1;
    } else {
      position.column += 1;
    }
  }

  return position;
}

#endif
