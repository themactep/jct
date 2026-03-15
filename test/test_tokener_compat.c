#include "json_config.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
  json_object *from_string = NULL;
  json_object *from_file = NULL;
  char path[] = "/tmp/jct-json-XXXXXX";
  int fd = -1;
  FILE *tmp = NULL;
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

  from_string = json_tokener_parse("{\"ok\":true}");
  ok &= expect(from_string != NULL, "json_tokener_parse handles complete JSON");
  ok &= expect(json_object_object_get_ex(from_string, "ok", &value),
               "json_tokener_parse result can be queried");
  ok &= expect(json_object_get_boolean(value) == 1,
               "json_tokener_parse preserves booleans");
  ok &= expect(json_object_put(from_string) == 1,
               "json_tokener_parse result cleanup succeeds");

  fd = mkstemp(path);
  ok &= expect(fd >= 0, "mkstemp creates a temporary file");
  if (fd >= 0) {
    tmp = fdopen(fd, "w");
    ok &= expect(tmp != NULL, "fdopen opens the temporary file");
    if (tmp) {
      ok &= expect(fputs("{\"from_file\":[1,2]}", tmp) >= 0,
                   "test JSON is written to file");
      ok &= expect(fclose(tmp) == 0, "temporary file closes cleanly");
      tmp = NULL;
    } else {
      close(fd);
    }
  }

  from_file = json_object_from_file(path);
  ok &= expect(from_file != NULL, "json_object_from_file parses valid JSON");
  ok &= expect(json_object_object_get_ex(from_file, "from_file", &value),
               "json_object_from_file result can be queried");
  ok &= expect(json_object_array_length(value) == 2,
               "json_object_from_file preserves arrays");
  ok &= expect(json_object_put(from_file) == 1,
               "json_object_from_file result cleanup succeeds");
  unlink(path);

  json_tokener_free(tok);
  return ok ? 0 : 1;
}
