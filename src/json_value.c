/**
 * json_value.c - Implementation of JSON value handling functions
 */

#include "json_config.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Creates a new JSON value of the specified type
 */
JsonValue *create_json_value(JsonType type) {
  JsonValue *value = (JsonValue *)malloc(sizeof(JsonValue));
  if (!value) {
    return NULL;
  }

  memset(value, 0, sizeof(JsonValue));
  value->type = type;

  return value;
}

JsonValue *create_json_integer_value(int64_t integer) {
  JsonValue *value = create_json_value(JSON_NUMBER);
  if (!value) {
    return NULL;
  }

  value->value.number.kind = JSON_NUMBER_INT;
  value->value.number.integer = integer;
  value->value.number.real = (double)integer;
  return value;
}

JsonValue *create_json_double_value(double real) {
  JsonValue *value = create_json_value(JSON_NUMBER);
  if (!value) {
    return NULL;
  }

  value->value.number.kind = JSON_NUMBER_DOUBLE;
  value->value.number.real = real;
  value->value.number.integer = 0;
  return value;
}

/**
 * Frees a JSON value and all its children
 */
void free_json_value(JsonValue *value) {
  if (!value) {
    return;
  }

  switch (value->type) {
  case JSON_STRING:
    free(value->value.string);
    break;
  case JSON_ARRAY: {
    JsonArrayItem *item = value->value.array_head;
    while (item) {
      JsonArrayItem *next = item->next;
      free_json_value(item->value);
      free(item);
      item = next;
    }
    break;
  }
  case JSON_OBJECT: {
    JsonKeyValue *kv = value->value.object_head;
    while (kv) {
      JsonKeyValue *next = kv->next;
      free(kv->key);
      free_json_value(kv->value);
      free(kv);
      kv = next;
    }
    break;
  }
  default:
    // Nothing to free for other types
    break;
  }

  free(value);
}

/**
 * Deeply clones a JSON value
 */
JsonValue *clone_json_value(const JsonValue *value) {
  if (!value)
    return NULL;
  JsonValue *out = create_json_value(value->type);
  if (!out)
    return NULL;
  switch (value->type) {
  case JSON_NULL:
    break;
  case JSON_BOOL:
    out->value.boolean = value->value.boolean;
    break;
  case JSON_NUMBER:
    out->value.number = value->value.number;
    break;
  case JSON_STRING:
    out->value.string =
        value->value.string ? strdup(value->value.string) : NULL;
    break;
  case JSON_ARRAY: {
    JsonArrayItem *it = value->value.array_head;
    while (it) {
      JsonValue *child = clone_json_value(it->value);
      if (!child || !add_to_array(out, child)) {
        if (child)
          free_json_value(child);
        free_json_value(out);
        return NULL;
      }
      it = it->next;
    }
    break;
  }
  case JSON_OBJECT: {
    JsonKeyValue *kv = value->value.object_head;
    while (kv) {
      JsonValue *child = clone_json_value(kv->value);
      if (!child || !add_to_object(out, kv->key ? kv->key : "", child)) {
        if (child)
          free_json_value(child);
        free_json_value(out);
        return NULL;
      }
      kv = kv->next;
    }
    break;
  }
  }
  return out;
}

bool json_number_is_integer(const JsonValue *value) {
  return value && value->type == JSON_NUMBER &&
         value->value.number.kind == JSON_NUMBER_INT;
}

int64_t json_number_get_integer(const JsonValue *value) {
  if (!value || value->type != JSON_NUMBER) {
    return 0;
  }

  if (value->value.number.kind == JSON_NUMBER_INT) {
    return value->value.number.integer;
  }

  return (int64_t)value->value.number.real;
}

double json_number_get_double(const JsonValue *value) {
  if (!value || value->type != JSON_NUMBER) {
    return 0.0;
  }

  if (value->value.number.kind == JSON_NUMBER_INT) {
    return (double)value->value.number.integer;
  }

  return value->value.number.real;
}

enum json_type json_object_get_type(const json_object *obj) {
  if (!obj) {
    return json_type_null;
  }

  switch (obj->type) {
  case JSON_NULL:
    return json_type_null;
  case JSON_BOOL:
    return json_type_boolean;
  case JSON_NUMBER:
    return json_number_is_integer(obj) ? json_type_int : json_type_double;
  case JSON_STRING:
    return json_type_string;
  case JSON_ARRAY:
    return json_type_array;
  case JSON_OBJECT:
    return json_type_object;
  }

  return json_type_null;
}

bool json_object_object_get_ex(const json_object *obj, const char *key,
                               json_object **value) {
  json_object *found = NULL;

  if (value) {
    *value = NULL;
  }

  if (!obj || obj->type != JSON_OBJECT || !key) {
    return false;
  }

  found = get_object_item((JsonValue *)obj, key);
  if (!found) {
    return false;
  }

  if (value) {
    *value = found;
  }

  return true;
}

size_t json_object_array_length(const json_object *obj) {
  if (!obj || obj->type != JSON_ARRAY) {
    return 0;
  }

  return (size_t)get_array_size((JsonValue *)obj);
}

json_object *json_object_array_get_idx(const json_object *obj, size_t index) {
  if (!obj || obj->type != JSON_ARRAY) {
    return NULL;
  }

  if (index > (size_t)INT_MAX) {
    return NULL;
  }

  return get_array_item((JsonValue *)obj, (int)index);
}

const char *json_object_get_string(const json_object *obj) {
  if (!obj) {
    return NULL;
  }

  if (obj->type == JSON_STRING) {
    return obj->value.string;
  }

  return NULL;
}

int json_object_get_boolean(const json_object *obj) {
  if (!obj) {
    return 0;
  }

  switch (obj->type) {
  case JSON_BOOL:
    return obj->value.boolean != 0;
  case JSON_NULL:
    return 0;
  case JSON_NUMBER:
    if (json_number_is_integer(obj)) {
      return json_number_get_integer(obj) != 0;
    }
    return json_number_get_double(obj) != 0.0;
  case JSON_STRING:
    return obj->value.string && obj->value.string[0] != '\0';
  case JSON_ARRAY:
  case JSON_OBJECT:
    return 1;
  }

  return 0;
}

int64_t json_object_get_int64(const json_object *obj) {
  if (!obj) {
    return 0;
  }

  if (obj->type == JSON_NUMBER) {
    return json_number_get_integer(obj);
  }

  return 0;
}

double json_object_get_double(const json_object *obj) {
  if (!obj) {
    return 0.0;
  }

  if (obj->type == JSON_NUMBER) {
    return json_number_get_double(obj);
  }

  return 0.0;
}

/**
 * Adds a key-value pair to a JSON object
 */
int add_to_object(JsonValue *object, const char *key, JsonValue *value) {
  if (!object || !key || !value || object->type != JSON_OBJECT) {
    return 0;
  }

  // Check if key already exists, if so, replace the value
  JsonKeyValue *kv = object->value.object_head;
  while (kv) {
    if (strcmp(kv->key, key) == 0) {
      free_json_value(kv->value);
      kv->value = value;
      return 1;
    }
    kv = kv->next;
  }

  // Create new key-value pair
  JsonKeyValue *new_kv = (JsonKeyValue *)malloc(sizeof(JsonKeyValue));
  if (!new_kv) {
    return 0;
  }

  new_kv->key = strdup(key);
  if (!new_kv->key) {
    free(new_kv);
    return 0;
  }

  new_kv->value = value;
  new_kv->next = object->value.object_head;
  object->value.object_head = new_kv;

  return 1;
}

/**
 * Adds a value to a JSON array
 */
int add_to_array(JsonValue *array, JsonValue *value) {
  if (!array || !value || array->type != JSON_ARRAY) {
    return 0;
  }

  JsonArrayItem *new_item = (JsonArrayItem *)malloc(sizeof(JsonArrayItem));
  if (!new_item) {
    return 0;
  }

  new_item->value = value;

  // Add to the end of the array
  if (!array->value.array_head) {
    new_item->next = NULL;
    array->value.array_head = new_item;
  } else {
    JsonArrayItem *item = array->value.array_head;
    while (item->next) {
      item = item->next;
    }
    new_item->next = NULL;
    item->next = new_item;
  }

  return 1;
}

/**
 * Gets an item from a JSON array by index
 */
JsonValue *get_array_item(JsonValue *array, int index) {
  if (!array || array->type != JSON_ARRAY || index < 0) {
    return NULL;
  }

  JsonArrayItem *item = array->value.array_head;
  int i = 0;

  while (item && i < index) {
    item = item->next;
    i++;
  }

  return item ? item->value : NULL;
}

/**
 * Gets the size of a JSON array
 */
int get_array_size(JsonValue *array) {
  if (!array || array->type != JSON_ARRAY) {
    return 0;
  }

  int size = 0;
  JsonArrayItem *item = array->value.array_head;

  while (item) {
    size++;
    item = item->next;
  }

  return size;
}

/**
 * Gets a value from a JSON object by key
 */
JsonValue *get_object_item(JsonValue *object, const char *key) {
  if (!object || !key || object->type != JSON_OBJECT) {
    return NULL;
  }

  JsonKeyValue *kv = object->value.object_head;

  while (kv) {
    if (strcmp(kv->key, key) == 0) {
      return kv->value;
    }
    kv = kv->next;
  }

  return NULL;
}
