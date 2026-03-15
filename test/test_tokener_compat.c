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
  json_object *scalar = NULL;
  int ok = 1;

  ok &= expect(tok != NULL, "json_tokener_new succeeds");
  ok &= expect(json_tokener_parse_ex(tok, "{\"a\":", 5) == NULL,
               "partial chunk does not parse yet");
  ok &= expect(json_tokener_get_error(tok) == json_tokener_continue,
               "partial chunk reports continue");
  root = json_tokener_parse_ex(tok, "1,\"b\":[2,3]}", 12);
  ok &= expect(root != NULL, "second chunk completes parse");
  ok &= expect(json_tokener_get_error(tok) == json_tokener_success,
               "completed parse reports success");
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

  ok &= expect(json_tokener_parse_ex(tok, "{]", 2) == NULL,
               "invalid JSON does not parse");
  ok &= expect(json_tokener_get_error(tok) == json_tokener_error_parse,
               "invalid JSON reports parse error");
  ok &= expect(strcmp(json_tokener_error_desc(json_tokener_get_error(tok)),
                      "parse error") == 0,
               "error description is stable");

  json_tokener_reset(tok);
  ok &= expect(json_tokener_get_error(tok) == json_tokener_success,
               "reset clears tokener error");
  scalar = json_tokener_parse_ex(tok, "true", 4);
  ok &= expect(scalar != NULL, "tokener can be reused after reset");
  ok &= expect(json_object_get_boolean(scalar) == 1,
               "reused tokener parses scalar values");
  ok &= expect(json_object_put(scalar) == 1, "scalar cleanup succeeds");

  json_tokener_free(tok);
  return ok ? 0 : 1;
}
