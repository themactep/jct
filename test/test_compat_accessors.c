#include "json_config.h"

#include <limits.h>
#include <math.h>
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
  const char *json =
      "{\"name\":\"cam\",\"enabled\":true,\"big\":9007199254740993,"
      "\"frac\":1.5,\"arr\":[1,2.5]}";
  json_object *root = parse_json_string(json);
  json_object *value = NULL;
  struct array_list *arr = NULL;
  int field_count = 0;
  int ok = 1;

  ok &= expect(root != NULL, "parse_json_string returns object");
  ok &= expect(json_object_get_type(root) == json_type_object,
               "root type is object");

  ok &= expect(json_object_object_get_ex(root, "name", &value),
               "object_get_ex finds name");
  ok &= expect(json_object_get_type(value) == json_type_string,
               "name type is string");
  ok &= expect(strcmp(json_object_get_string(value), "cam") == 0,
               "name value matches");

  ok &= expect(json_object_object_get_ex(root, "enabled", &value),
               "object_get_ex finds enabled");
  ok &= expect(json_object_get_boolean(value) == 1, "enabled is true");

  ok &= expect(json_object_object_get_ex(root, "big", &value),
               "object_get_ex finds big");
  ok &= expect(json_object_get_type(value) == json_type_int,
               "big type is integer");
  ok &= expect(json_object_get_int(value) == INT_MAX,
               "get_int clamps large integers like json-c");
  ok &= expect(json_object_get_int64(value) == 9007199254740993LL,
               "big integer preserved exactly");

  ok &= expect(json_object_object_get_ex(root, "frac", &value),
               "object_get_ex finds frac");
  ok &= expect(json_object_get_type(value) == json_type_double,
               "frac type is double");
  ok &= expect(fabs(json_object_get_double(value) - 1.5) < 1e-12,
               "frac double preserved");

  ok &= expect(json_object_object_get_ex(root, "arr", &value),
               "object_get_ex finds arr");
  ok &= expect(json_object_get_type(value) == json_type_array,
               "arr type is array");
  ok &= expect(json_object_array_length(value) == 2, "array length matches");
  arr = json_object_get_array(value);
  ok &= expect(arr != NULL, "json_object_get_array returns array_list");
  ok &= expect(array_list_length(arr) == 2, "array_list_length matches");

  ok &= expect(json_object_get_int64(json_object_array_get_idx(value, 0)) == 1,
               "array index 0 integer matches");
  ok &= expect(fabs(json_object_get_double(json_object_array_get_idx(value, 1)) -
                    2.5) < 1e-12,
               "array index 1 double matches");
  ok &= expect(json_object_get_int64(array_list_get_idx(arr, 0)) == 1,
               "array_list_get_idx returns array values");

  json_object_object_foreach(root, key, child) {
    (void)key;
    (void)child;
    field_count++;
  }
  ok &= expect(field_count == 5, "object_foreach iterates all fields");

  free_json_value(root);
  return ok ? 0 : 1;
}
