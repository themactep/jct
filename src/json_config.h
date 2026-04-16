/**
 * json_config.h - Header file for the JSON configuration CLI tool
 */

#ifndef JSON_CONFIG_H
#define JSON_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// JSON value types
typedef enum {
  JSON_NULL,
  JSON_BOOL,
  JSON_NUMBER,
  JSON_STRING,
  JSON_ARRAY,
  JSON_OBJECT
} JsonType;

// Forward declaration for JsonValue/json_object compatibility
typedef struct json_object JsonValue;
typedef struct json_object json_object;
typedef struct json_tokener json_tokener;
struct array_list;

typedef enum json_tokener_error {
  json_tokener_success = 0,
  json_tokener_continue,
  json_tokener_error_parse,
  json_tokener_error_memory
} json_tokener_error;

typedef enum json_type {
  json_type_null,
  json_type_boolean,
  json_type_double,
  json_type_int,
  json_type_object,
  json_type_array,
  json_type_string
} json_type;

#define JSON_C_TO_STRING_PLAIN 0
#define JSON_C_TO_STRING_PRETTY (1 << 1)
#define JSON_C_TO_STRING_NOSLASHESCAPE (1 << 4)

typedef enum {
  JSON_NUMBER_INT,
  JSON_NUMBER_DOUBLE
} JsonNumberKind;

typedef struct JsonNumberValue {
  JsonNumberKind kind;
  int64_t integer;
  double real;
} JsonNumberValue;

// Structure for key-value pairs in objects
typedef struct JsonKeyValue {
  char *key;
  JsonValue *value;
  struct JsonKeyValue *next;
} JsonKeyValue;

// Structure for array elements
typedef struct JsonArrayItem {
  JsonValue *value;
  struct JsonArrayItem *next;
} JsonArrayItem;

// Structure for JSON values
struct json_object {
  JsonType type;
  int refcount;
  union {
    int boolean;
    JsonNumberValue number;
    char *string;
    JsonArrayItem *array_head;
    JsonKeyValue *object_head;
  } value;
  struct array_list *array_view;
  char *serialized_cache;
};

struct array_list {
  json_object *array;
};

// JSON value functions
JsonValue *create_json_value(JsonType type);
JsonValue *create_json_integer_value(int64_t integer);
JsonValue *create_json_double_value(double real);
void free_json_value(JsonValue *value);
int add_to_object(JsonValue *object, const char *key, JsonValue *value);
int add_to_array(JsonValue *array, JsonValue *value);
JsonValue *get_array_item(JsonValue *array, int index);
int get_array_size(JsonValue *array);
JsonValue *get_object_item(JsonValue *object, const char *key);
bool json_number_is_integer(const JsonValue *value);
int64_t json_number_get_integer(const JsonValue *value);
double json_number_get_double(const JsonValue *value);
enum json_type json_object_get_type(const json_object *obj);
int json_object_is_type(const json_object *obj, enum json_type type);
bool json_object_object_get_ex(const json_object *obj, const char *key,
                               json_object **value);
size_t json_object_array_length(const json_object *obj);
json_object *json_object_array_get_idx(const json_object *obj, size_t index);
const char *json_object_get_string(const json_object *obj);
int json_object_get_boolean(const json_object *obj);
int json_object_get_int(const json_object *obj);
int64_t json_object_get_int64(const json_object *obj);
double json_object_get_double(const json_object *obj);
json_object *json_object_get(json_object *obj);
int json_object_put(json_object *obj);
json_object *json_object_new_object(void);
json_object *json_object_new_array(void);
json_object *json_object_new_string(const char *value);
json_object *json_object_new_int(int32_t value);
json_object *json_object_new_int64(int64_t value);
json_object *json_object_new_double(double value);
json_object *json_object_new_boolean(int value);
json_object *json_object_new_null(void);
int json_object_object_add(json_object *obj, const char *key, json_object *val);
void json_object_object_del(json_object *obj, const char *key);
int json_object_array_add(json_object *obj, json_object *val);
struct array_list *json_object_get_array(const json_object *obj);
int array_list_length(struct array_list *arr);
json_object *array_list_get_idx(struct array_list *arr, int idx);

#define json_object_object_foreach(obj, key, val)                               \
  for (JsonKeyValue *_json_kv =                                                 \
           ((obj) && (obj)->type == JSON_OBJECT) ? (obj)->value.object_head     \
                                                : NULL;                         \
       _json_kv != NULL; _json_kv = _json_kv->next)                             \
    for (const char *key = _json_kv->key; key != NULL; key = NULL)              \
      for (json_object *val = _json_kv->value; val != NULL; val = NULL)

// JSON parsing functions
JsonValue *parse_json_file(const char *filepath);
// Parse from a JSON string buffer
JsonValue *parse_json_string(const char *json_str);
json_tokener *json_tokener_new(void);
void json_tokener_free(json_tokener *tok);
json_object *json_tokener_parse_ex(json_tokener *tok, const char *str, int len);
enum json_tokener_error json_tokener_get_error(const json_tokener *tok);
const char *json_tokener_error_desc(enum json_tokener_error err);
void json_tokener_reset(json_tokener *tok);
json_object *json_tokener_parse(const char *str);
json_object *json_object_from_file(const char *filepath);

// JSON serialization functions
char *json_to_string(JsonValue *json, int pretty);
const char *json_object_to_json_string(json_object *obj);
const char *json_object_to_json_string_ext(json_object *obj, int flags);
int json_object_to_file_ext(const char *filename, json_object *obj, int flags);

// Deep clone a JSON value (recursively)
JsonValue *clone_json_value(const JsonValue *value);

// Config manipulation functions
JsonValue *load_config(const char *filepath);
int save_config(const char *filepath, JsonValue *json);
JsonValue *get_nested_item(JsonValue *object, const char *key);
int set_nested_item(JsonValue *object, const char *key, const char *value_str);
int merge_json_into(JsonValue **dest_ptr, const JsonValue *src);
JsonValue *diff_json(const JsonValue *modified, const JsonValue *original);
void print_item(JsonValue *item);

#ifdef __cplusplus
}
#endif

#endif /* JSON_CONFIG_H */
