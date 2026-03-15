#include "json_config.h"

#include <stdio.h>
#include <string.h>

static int expect(int cond, const char *msg) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", msg);
    return 0;
  }
  return 1;
}

int main(void) {
  json_tokener *tok = json_tokener_new();
  json_object *root = NULL;
  json_object *dup = NULL;
  json_object *value = NULL;
  int ok = 1;

  ok &= expect(tok != NULL, "json_tokener_new succeeds");
  ok &= expect(json_tokener_parse_ex(tok, "{\"a\":", 5) == NULL,
               "partial chunk does not parse yet");
  root = json_tokener_parse_ex(tok, "1,\"b\":[2,3]}", 12);
  ok &= expect(root != NULL, "second chunk completes parse");
  ok &= expect(json_object_object_get_ex(root, "a", &value),
               "parsed object contains field a");
  ok &= expect(json_object_get_int64(value) == 1, "field a value matches");

  dup = json_object_get(root);
  ok &= expect(dup == root, "json_object_get returns same pointer");
  ok &= expect(json_object_put(root) == 1, "first json_object_put succeeds");
  ok &= expect(json_object_object_get_ex(dup, "b", &value),
               "retained reference still valid");
  ok &= expect(json_object_array_length(value) == 2, "array field length matches");
  ok &= expect(json_object_put(dup) == 1, "second json_object_put succeeds");

  json_tokener_free(tok);
  return ok ? 0 : 1;
}
