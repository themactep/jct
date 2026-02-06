/**
 * json_config.h - Header file for the JSON configuration CLI tool
 */

#ifndef JSON_CONFIG_H
#define JSON_CONFIG_H

#include <stddef.h>

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

// Forward declaration for JsonValue
typedef struct JsonValue JsonValue;

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
struct JsonValue {
  JsonType type;
  union {
    int boolean;
    double number;
    char *string;
    JsonArrayItem *array_head;
    JsonKeyValue *object_head;
  } value;
};

// JSON value functions
JsonValue *create_json_value(JsonType type);
void free_json_value(JsonValue *value);
int add_to_object(JsonValue *object, const char *key, JsonValue *value);
int add_to_array(JsonValue *array, JsonValue *value);
JsonValue *get_array_item(JsonValue *array, int index);
int get_array_size(JsonValue *array);
JsonValue *get_object_item(JsonValue *object, const char *key);

// JSON parsing functions
JsonValue *parse_json_file(const char *filepath);
// Parse from a JSON string buffer
JsonValue *parse_json_string(const char *json_str);

// JSON serialization functions
char *json_to_string(JsonValue *json, int pretty);

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
