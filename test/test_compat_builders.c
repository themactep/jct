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
  json_object *root = json_object_new_object();
  json_object *array = json_object_new_array();
  json_object *value = NULL;
  const char *serialized = NULL;
  int ok = 1;

  ok &= expect(root != NULL, "json_object_new_object succeeds");
  ok &= expect(array != NULL, "json_object_new_array succeeds");
  ok &= expect(json_object_is_type(root, json_type_object),
               "json_object_is_type detects objects");
  ok &= expect(json_object_object_add(root, "name",
                                      json_object_new_string("cam")) == 0,
               "json_object_object_add accepts strings");
  ok &= expect(json_object_object_add(root, "enabled",
                                      json_object_new_boolean(1)) == 0,
               "json_object_object_add accepts booleans");
  ok &= expect(json_object_object_add(root, "big",
                                      json_object_new_int64(9007199254740993LL)) == 0,
               "json_object_object_add accepts int64");
  ok &= expect(json_object_array_add(array, json_object_new_double(2.5)) == 0,
               "json_object_array_add accepts doubles");
  ok &= expect(json_object_array_add(array, NULL) == 0,
               "json_object_array_add maps NULL to JSON null");
  ok &= expect(json_object_object_add(root, "values", array) == 0,
               "json_object_object_add accepts arrays");
  ok &= expect(json_object_object_add(root, "nothing", NULL) == 0,
               "json_object_object_add maps NULL to JSON null");

  ok &= expect(json_object_object_get_ex(root, "values", &value),
               "object contains array field");
  ok &= expect(json_object_array_length(value) == 2,
               "array length includes null placeholder");
  ok &= expect(json_object_get_type(json_object_array_get_idx(value, 1)) ==
                   json_type_null,
               "NULL entry serializes as json null");

  serialized = json_object_to_json_string(root);
  ok &= expect(serialized != NULL, "json_object_to_json_string succeeds");
  ok &= expect(strstr(serialized, "\"name\":\"cam\"") != NULL,
               "serialized string contains name");
  ok &= expect(strstr(serialized, "\"enabled\":true") != NULL,
               "serialized string contains boolean");
  ok &= expect(strstr(serialized, "\"big\":9007199254740993") != NULL,
               "serialized string preserves integer");
  ok &= expect(strstr(serialized, "\"values\":[2.5,null]") != NULL,
               "serialized string contains array and null");
  ok &= expect(strstr(serialized, "\"nothing\":null") != NULL,
               "serialized string contains object null");

  ok &= expect(json_object_put(root) == 1, "json_object_put cleans up builders");
  return ok ? 0 : 1;
}
