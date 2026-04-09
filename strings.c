#include "slimcc.h"

void strarray_push(StringArray *arr, const char *s) {
  if (!arr->data) {
    arr->data = calloc(8, sizeof(char *));
    arr->capacity = 8;
  }

  if (arr->capacity == arr->len) {
    arr->data = realloc(arr->data, sizeof(char *) * arr->capacity * 2);
    arr->capacity *= 2;
    for (int i = arr->len; i < arr->capacity; i++)
      arr->data[i] = NULL;
  }

  arr->data[arr->len++] = (char*) s;
}

// Takes a printf-style format string and returns a formatted string.
char *format(char *fmt, ...) {
  char *buf = NULL;
  int buflen;
  
  va_list ap;
  va_list copy;
  va_start(ap, fmt);
  va_copy(copy, ap);
  
  buflen = vsnprintf(buf, 0, fmt, ap);
  buf = malloc(buflen + 1);
  vsnprintf(buf, buflen + 1, fmt, copy);
  va_end(ap);
  va_end(copy);
  
  return buf;
}
