/**
 * json_config.c - Implementation of JSON configuration manipulation functions
 */

#include "json_config.h"
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for getpid() and unlink()

/**
 * Loads JSON data from a file path
 *
 * @param filepath Path to the JSON file
 * @return Pointer to JsonValue or NULL on error
 */
JsonValue *load_config(const char *filepath) {
  return parse_json_file(filepath);
}

/**
 * Compare function for sorting JSON keys alphabetically
 */
static int compare_json_keys(const void *a, const void *b) {
  const JsonKeyValue *kv_a = *(const JsonKeyValue **)a;
  const JsonKeyValue *kv_b = *(const JsonKeyValue **)b;

  if (!kv_a->key && !kv_b->key)
    return 0;
  if (!kv_a->key)
    return -1;
  if (!kv_b->key)
    return 1;

  return strcmp(kv_a->key, kv_b->key);
}

/**
 * Writes a JSON value directly to a file with proper indentation
 */
static int write_json_to_file(FILE *file, JsonValue *json, int indent) {
#ifdef DEBUG
  fprintf(stderr,
          "DEBUG: write_json_to_file called with file=%p, json=%p, indent=%d\n",
          (void *)file, (void *)json, indent);
#endif

  if (!file || !json) {
#ifdef DEBUG
    fprintf(stderr,
            "DEBUG: write_json_to_file - null parameter: file=%p, json=%p\n",
            (void *)file, (void *)json);
#endif
    return 0;
  }

  // Additional safety check - verify the json pointer is valid
  // Check if the type field is within valid range
#ifdef DEBUG
  fprintf(stderr, "DEBUG: write_json_to_file - checking json type at %p\n",
          (void *)json);

  // Read the raw bytes to see what's actually there
  unsigned char *raw_bytes = (unsigned char *)json;
  fprintf(stderr,
          "DEBUG: Raw bytes at json address: %02x %02x %02x %02x %02x %02x "
          "%02x %02x\n",
          raw_bytes[0], raw_bytes[1], raw_bytes[2], raw_bytes[3], raw_bytes[4],
          raw_bytes[5], raw_bytes[6], raw_bytes[7]);
#endif

  if (json->type < JSON_NULL || json->type > JSON_OBJECT) {
    fprintf(stderr, "Error: Invalid JSON type %d (0x%x) at address %p\n",
            json->type, json->type, (void *)json);
    fprintf(stderr,
            "Error: This looks like corrupted memory containing: '%.8s'\n",
            (char *)json);
    return 0;
  }

#ifdef DEBUG
  fprintf(stderr, "DEBUG: write_json_to_file - json type is valid: %d\n",
          json->type);
#endif
  int success = 1;

  switch (json->type) {
  case JSON_NULL:
    success = (fprintf(file, "null") > 0);
    break;
  case JSON_BOOL:
    success = (fprintf(file, "%s", json->value.boolean ? "true" : "false") > 0);
    break;
  case JSON_NUMBER:
    // Check if it's an integer
    if (json->value.number == (int64_t)json->value.number) {
      success = (fprintf(file, "%" PRId64, (int64_t)json->value.number) > 0);
    } else {
      success = (fprintf(file, "%g", json->value.number) > 0);
    }
    break;
  case JSON_STRING:
    if (json->value.string) {
      // Simple string escaping for common characters
      success = (fprintf(file, "\"") > 0);
      for (const char *p = json->value.string; *p; p++) {
        switch (*p) {
        case '\\':
          success = success && (fprintf(file, "\\\\") > 0);
          break;
        case '\"':
          success = success && (fprintf(file, "\\\"") > 0);
          break;
        case '\b':
          success = success && (fprintf(file, "\\b") > 0);
          break;
        case '\f':
          success = success && (fprintf(file, "\\f") > 0);
          break;
        case '\n':
          success = success && (fprintf(file, "\\n") > 0);
          break;
        case '\r':
          success = success && (fprintf(file, "\\r") > 0);
          break;
        case '\t':
          success = success && (fprintf(file, "\\t") > 0);
          break;
        default:
          if ((unsigned char)*p < 32) {
            success =
                success && (fprintf(file, "\\u%04x", (unsigned char)*p) > 0);
          } else {
            success = success && (fprintf(file, "%c", *p) > 0);
          }
          break;
        }
        if (!success)
          break;
      }
      success = success && (fprintf(file, "\"") > 0);
    } else {
      success = (fprintf(file, "\"\"") > 0);
    }
    break;
  case JSON_OBJECT: {
    if (!json->value.object_head) {
      success = (fprintf(file, "{}") > 0);
      break;
    }

    // Count the number of key-value pairs
    int count = 0;
    JsonKeyValue *kv = json->value.object_head;
    while (kv) {
      count++;
      kv = kv->next;
    }

    // Create an array of pointers to key-value pairs
    JsonKeyValue **kvs =
        (JsonKeyValue **)malloc(count * sizeof(JsonKeyValue *));
    if (!kvs) {
      fprintf(stderr,
              "Error: Memory allocation failed for sorting JSON keys.\n");
      return 0;
    }

    // Fill the array
    kv = json->value.object_head;
    for (int i = 0; i < count; i++) {
      kvs[i] = kv;
      kv = kv->next;
    }

    // Sort the array alphabetically by key
    qsort(kvs, count, sizeof(JsonKeyValue *), compare_json_keys);

    // Debug: Show all keys after sorting
#ifdef DEBUG
    fprintf(stderr, "DEBUG: After sorting, found %d keys:\n", count);
    for (int i = 0; i < count; i++) {
      fprintf(stderr, "DEBUG: kvs[%d]: key='%s' at %p, value=%p\n", i,
              kvs[i]->key ? kvs[i]->key : "NULL", (void *)kvs[i]->key,
              (void *)kvs[i]->value);
    }
#endif

    // Write the sorted key-value pairs
    success = (fprintf(file, "{\n") > 0);
    int first = 1;

    for (int i = 0; i < count && success; i++) {
      if (!first) {
        success = success && (fprintf(file, ",\n") > 0);
      }

      // Print indentation
      for (int j = 0; j < indent + 1 && success; j++) {
        success = success && (fprintf(file, "  ") > 0);
      }

      // Print key with detailed debugging
#ifdef DEBUG
      fprintf(stderr, "DEBUG: Processing key '%s' at kvs[%d]\n",
              kvs[i]->key ? kvs[i]->key : "NULL", i);
      fprintf(stderr, "DEBUG: kvs[%d] structure: kvs=%p, key=%p, value=%p\n", i,
              (void *)kvs[i], (void *)kvs[i]->key, (void *)kvs[i]->value);

      // Check if key pointer looks valid
      if (kvs[i]->key) {
        unsigned char *key_bytes = (unsigned char *)kvs[i]->key;
        fprintf(stderr,
                "DEBUG: Key bytes at %p: %02x %02x %02x %02x %02x %02x "
                "%02x %02x\n",
                (void *)kvs[i]->key, key_bytes[0], key_bytes[1], key_bytes[2],
                key_bytes[3], key_bytes[4], key_bytes[5], key_bytes[6],
                key_bytes[7]);
      }
#endif

      success = success &&
                (fprintf(file, "\"%s\": ", kvs[i]->key ? kvs[i]->key : "") > 0);

      // Print value
      if (kvs[i]->value) {
#ifdef DEBUG
        fprintf(stderr,
                "DEBUG: About to recursively process value for key '%s'\n",
                kvs[i]->key ? kvs[i]->key : "NULL");
#endif
        success =
            success && write_json_to_file(file, kvs[i]->value, indent + 1);
      } else {
        success = success && (fprintf(file, "null") > 0);
      }

      first = 0;
    }

    // Free the array
    free(kvs);

    if (success) {
      success = (fprintf(file, "\n") > 0);
      // Print indentation for closing brace
      for (int i = 0; i < indent && success; i++) {
        success = success && (fprintf(file, "  ") > 0);
      }
      success = success && (fprintf(file, "}") > 0);
    }
    break;
  }
  case JSON_ARRAY: {
    if (!json->value.array_head) {
      success = (fprintf(file, "[]") > 0);
      break;
    }

    success = (fprintf(file, "[\n") > 0);
    JsonArrayItem *item = json->value.array_head;
    int first = 1;

    while (item && success) {
      if (!first) {
        success = success && (fprintf(file, ",\n") > 0);
      }

      // Print indentation
      for (int i = 0; i < indent + 1 && success; i++) {
        success = success && (fprintf(file, "  ") > 0);
      }

      // Print value
      if (item->value) {
        success = success && write_json_to_file(file, item->value, indent + 1);
      } else {
        success = success && (fprintf(file, "null") > 0);
      }

      first = 0;
      item = item->next;
    }

    if (success) {
      success = (fprintf(file, "\n") > 0);
      // Print indentation for closing bracket
      for (int i = 0; i < indent && success; i++) {
        success = success && (fprintf(file, "  ") > 0);
      }
      success = success && (fprintf(file, "]") > 0);
    }
    break;
  }
  default:
    fprintf(stderr, "Error: Unknown JSON type.\n");
    success = 0;
    break;
  }

  return success;
}

/**
 * Saves JSON data to a file path
 *
 * @param filepath Path to save the JSON file
 * @param json The JSON object to save
 * @return 1 on success, 0 on failure
 */
int save_config(const char *filepath, JsonValue *json) {
#ifdef DEBUG
  fprintf(stderr, "DEBUG: save_config called with filepath='%s', json=%p\n",
          filepath ? filepath : "NULL", (void *)json);
#endif

  if (!json) {
#ifdef DEBUG
    fprintf(stderr, "DEBUG: save_config - json is NULL, returning 0\n");
#endif
    return 0;
  }

#ifdef DEBUG
  fprintf(stderr, "DEBUG: save_config - json type=%d\n", json->type);
#endif

  // Create temporary file path
  char temp_filepath[512];
  snprintf(temp_filepath, sizeof(temp_filepath),
           "/tmp/prudynt_config_temp_%d.json", getpid());
#ifdef DEBUG
  fprintf(stderr, "DEBUG: save_config - using temporary file: %s\n",
          temp_filepath);
#endif

  FILE *file = fopen(temp_filepath, "w");
  if (!file) {
    fprintf(stderr,
            "Error: Failed to open temporary file '%s' for writing: %s\n",
            temp_filepath, strerror(errno));
    return 0;
  }

#ifdef DEBUG
  fprintf(stderr, "DEBUG: save_config - about to call write_json_to_file\n");
#endif
  int success = write_json_to_file(file, json, 0);
#ifdef DEBUG
  fprintf(stderr, "DEBUG: save_config - write_json_to_file returned %d\n",
          success);
#endif

  // Add a final newline
  if (success) {
    success = (fprintf(file, "\n") > 0);
#ifdef DEBUG
    fprintf(stderr, "DEBUG: save_config - added final newline, success=%d\n",
            success);
#endif
  }

  fclose(file);

  if (!success) {
    fprintf(stderr,
            "Error: Failed to write complete JSON to temporary file '%s'.\n",
            temp_filepath);
    unlink(temp_filepath); // Remove the failed temporary file
    return 0;
  }

  // Atomically replace the original file with the temporary file
#ifdef DEBUG
  fprintf(stderr, "DEBUG: save_config - attempting to rename '%s' to '%s'\n",
          temp_filepath, filepath);
#endif
  if (rename(temp_filepath, filepath) != 0) {
    if (errno == EXDEV) {
      // Cross-device link error - need to copy and delete instead
#ifdef DEBUG
      fprintf(stderr,
              "DEBUG: Cross-device rename failed, trying copy method\n");
#endif

      FILE *src = fopen(temp_filepath, "r");
      if (!src) {
        fprintf(stderr,
                "Error: Failed to open temporary file for copying: %s\n",
                strerror(errno));
        unlink(temp_filepath);
        return 0;
      }

      FILE *dst = fopen(filepath, "w");
      if (!dst) {
        fprintf(stderr,
                "Error: Failed to open destination file for copying: %s\n",
                strerror(errno));
        fclose(src);
        unlink(temp_filepath);
        return 0;
      }

      // Copy the file contents
      char buffer[4096];
      size_t bytes;
      int copy_success = 1;
      while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
          fprintf(stderr, "Error: Failed to write during copy: %s\n",
                  strerror(errno));
          copy_success = 0;
          break;
        }
      }

      fclose(src);
      fclose(dst);

      if (!copy_success) {
        unlink(temp_filepath);
        return 0;
      }

      // Remove the temporary file
      unlink(temp_filepath);
#ifdef DEBUG
      fprintf(stderr, "DEBUG: save_config - successfully copied temp file to "
                      "destination\n");
#endif
    } else {
      fprintf(stderr,
              "Error: Failed to rename temporary file '%s' to '%s': %s\n",
              temp_filepath, filepath, strerror(errno));
      unlink(temp_filepath); // Clean up the temporary file
      return 0;
    }
  } else {
#ifdef DEBUG
    fprintf(
        stderr,
        "DEBUG: save_config - successfully renamed temp file to destination\n");
#endif
  }

#ifdef DEBUG
  fprintf(stderr,
          "DEBUG: save_config - completed successfully with atomic rename\n");
#endif
  return 1;
}

// Helper that merges src object members into dest object recursively.
static int merge_object_into(JsonValue *dest_obj, const JsonValue *src_obj) {
  if (!dest_obj || !src_obj || dest_obj->type != JSON_OBJECT ||
      src_obj->type != JSON_OBJECT) {
    return 0;
  }

  JsonKeyValue *kv = src_obj->value.object_head;
  while (kv) {
    const char *key = kv->key ? kv->key : "";
    JsonValue *dest_child = get_object_item(dest_obj, key);
    JsonValue *src_child = kv->value;

    if (dest_child && src_child && dest_child->type == JSON_OBJECT &&
        src_child->type == JSON_OBJECT) {
      if (!merge_object_into(dest_child, src_child)) {
        return 0;
      }
    } else {
      JsonValue *replacement = src_child ? clone_json_value(src_child)
                                         : create_json_value(JSON_NULL);
      if (!replacement) {
        return 0;
      }
      if (!add_to_object(dest_obj, key, replacement)) {
        free_json_value(replacement);
        return 0;
      }
    }

    kv = kv->next;
  }

  return 1;
}

int merge_json_into(JsonValue **dest_ptr, const JsonValue *src) {
  if (!dest_ptr || !src) {
    return 0;
  }

  if (!*dest_ptr) {
    JsonValue *clone = clone_json_value(src);
    if (!clone) {
      return 0;
    }
    *dest_ptr = clone;
    return 1;
  }

  JsonValue *dest = *dest_ptr;

  if (dest->type == JSON_OBJECT && src->type == JSON_OBJECT) {
    return merge_object_into(dest, src);
  }

  JsonValue *replacement = clone_json_value(src);
  if (!replacement) {
    return 0;
  }

  free_json_value(dest);
  *dest_ptr = replacement;
  return 1;
}

// Helper to check if two JSON values are equal
static int json_values_equal(const JsonValue *a, const JsonValue *b) {
  if (!a && !b) {
    return 1;
  }
  if (!a || !b) {
    return 0;
  }
  if (a->type != b->type) {
    return 0;
  }

  switch (a->type) {
  case JSON_NULL:
    return 1;
  case JSON_BOOL:
    return a->value.boolean == b->value.boolean;
  case JSON_NUMBER:
    return a->value.number == b->value.number;
  case JSON_STRING:
    if (!a->value.string && !b->value.string) {
      return 1;
    }
    if (!a->value.string || !b->value.string) {
      return 0;
    }
    return strcmp(a->value.string, b->value.string) == 0;
  case JSON_ARRAY: {
    int size_a = get_array_size((JsonValue *)a);
    int size_b = get_array_size((JsonValue *)b);
    if (size_a != size_b) {
      return 0;
    }
    for (int i = 0; i < size_a; i++) {
      JsonValue *item_a = get_array_item((JsonValue *)a, i);
      JsonValue *item_b = get_array_item((JsonValue *)b, i);
      if (!json_values_equal(item_a, item_b)) {
        return 0;
      }
    }
    return 1;
  }
  case JSON_OBJECT: {
    JsonKeyValue *kv_a = a->value.object_head;
    while (kv_a) {
      const char *key = kv_a->key ? kv_a->key : "";
      JsonValue *val_b = get_object_item((JsonValue *)b, key);
      if (!json_values_equal(kv_a->value, val_b)) {
        return 0;
      }
      kv_a = kv_a->next;
    }
    JsonKeyValue *kv_b = b->value.object_head;
    while (kv_b) {
      const char *key = kv_b->key ? kv_b->key : "";
      JsonValue *val_a = get_object_item((JsonValue *)a, key);
      if (!val_a) {
        return 0;
      }
      kv_b = kv_b->next;
    }
    return 1;
  }
  }
  return 0;
}

// Helper that diffs objects recursively
static JsonValue *diff_objects(const JsonValue *modified_obj,
                               const JsonValue *original_obj) {
  if (!modified_obj || modified_obj->type != JSON_OBJECT) {
    return NULL;
  }

  JsonValue *diff = create_json_value(JSON_OBJECT);
  if (!diff) {
    return NULL;
  }

  JsonKeyValue *kv = modified_obj->value.object_head;
  while (kv) {
    const char *key = kv->key ? kv->key : "";
    JsonValue *modified_child = kv->value;
    JsonValue *original_child =
        original_obj ? get_object_item((JsonValue *)original_obj, key) : NULL;

    // If key doesn't exist in original, include it
    if (!original_child) {
      JsonValue *cloned = clone_json_value(modified_child);
      if (cloned) {
        add_to_object(diff, key, cloned);
      }
    }
    // If both are objects, recursively diff them
    else if (modified_child && modified_child->type == JSON_OBJECT &&
             original_child->type == JSON_OBJECT) {
      JsonValue *child_diff = diff_objects(modified_child, original_child);
      if (child_diff) {
        // Only include if the child diff is not empty
        if (child_diff->value.object_head != NULL) {
          add_to_object(diff, key, child_diff);
        } else {
          free_json_value(child_diff);
        }
      }
    }
    // If values are different, include the modified value
    else if (!json_values_equal(modified_child, original_child)) {
      JsonValue *cloned = clone_json_value(modified_child);
      if (cloned) {
        add_to_object(diff, key, cloned);
      }
    }

    kv = kv->next;
  }

  return diff;
}

/**
 * Computes the difference between two JSON values
 *
 * @param modified The modified/current JSON value
 * @param original The original/base JSON value to compare against
 * @return A new JSON value containing only the differences, or NULL on error
 */
JsonValue *diff_json(const JsonValue *modified, const JsonValue *original) {
  if (!modified) {
    return NULL;
  }

  // If no original, return a clone of modified
  if (!original) {
    return clone_json_value(modified);
  }

  // If both are objects, perform recursive diff
  if (modified->type == JSON_OBJECT && original->type == JSON_OBJECT) {
    return diff_objects(modified, original);
  }

  // If they're not both objects, check if they're equal
  if (json_values_equal(modified, original)) {
    // Return empty object if equal
    return create_json_value(JSON_OBJECT);
  }

  // If different types or different values, return the modified value
  return clone_json_value(modified);
}

/**
 * Gets a nested item using dot notation
 *
 * @param object The JSON object to search in
 * @param key The key path using dot notation (e.g., "section.key")
 * @return Pointer to JsonValue or NULL if not found
 */
JsonValue *get_nested_item(JsonValue *object, const char *key) {
  if (!object || !key) {
    return NULL;
  }

  char *key_copy = strdup(key);
  if (!key_copy) {
    fprintf(stderr, "Error: Memory allocation failed for key copy.\n");
    return NULL;
  }

  JsonValue *current = object;
  char *token = strtok(key_copy, ".");

  while (token != NULL && current != NULL) {
    if (current->type == JSON_OBJECT) {
      current = get_object_item(current, token);
    } else if (current->type == JSON_ARRAY) {
      char *endptr;
      long index = strtol(token, &endptr, 10);
      if (*endptr != '\0' || index < 0 || index >= get_array_size(current)) {
        fprintf(stderr, "Error: Invalid array index '%s' for key '%s'.\n",
                token, key);
        free(key_copy);
        return NULL;
      }
      current = get_array_item(current, index);
    } else {
      // Cannot traverse further
      free(key_copy);
      return NULL;
    }
    token = strtok(NULL, ".");
  }

  free(key_copy);
  return current;
}

/**
 * Sets a nested item using dot notation
 *
 * @param object The JSON object to modify
 * @param key The key path using dot notation (e.g., "section.key")
 * @param value_str The string representation of the value to set
 * @return 1 on success, 0 on failure
 */
int set_nested_item(JsonValue *object, const char *key, const char *value_str) {
  if (!object || !key || !value_str) {
    return 0;
  }

  char *key_copy = strdup(key);
  if (!key_copy) {
    fprintf(stderr, "Error: Memory allocation failed for key copy.\n");
    return 0;
  }

  // Split the key into parts
  char *parts[256]; // Assuming a reasonable max depth
  int part_count = 0;
  char *token = strtok(key_copy, ".");

  while (token != NULL && part_count < 256) {
    parts[part_count++] = token;
    token = strtok(NULL, ".");
  }

  if (part_count == 0) {
    free(key_copy);
    return 0;
  }

  // Navigate to the parent object
  JsonValue *current = object;
  int i;

  for (i = 0; i < part_count - 1; i++) {
    JsonValue *next = NULL;

    if (current->type == JSON_OBJECT) {
      next = get_object_item(current, parts[i]);
      if (!next) {
        // Create intermediate object if it doesn't exist
        next = create_json_value(JSON_OBJECT);
        if (!next) {
          fprintf(stderr,
                  "Error: Failed to create intermediate object for key '%s'.\n",
                  parts[i]);
          free(key_copy);
          return 0;
        }
        add_to_object(current, parts[i], next);
      }
      current = next;
    } else if (current->type == JSON_ARRAY) {
      char *endptr;
      long index = strtol(parts[i], &endptr, 10);
      if (*endptr != '\0' || index < 0) {
        fprintf(stderr, "Error: Invalid array index '%s' for key '%s'.\n",
                parts[i], key);
        free(key_copy);
        return 0;
      }

      // Extend array if needed
      while (index >= get_array_size(current)) {
        JsonValue *new_obj = create_json_value(JSON_OBJECT);
        if (!new_obj) {
          fprintf(stderr, "Error: Failed to create new array item.\n");
          free(key_copy);
          return 0;
        }
        add_to_array(current, new_obj);
      }

      current = get_array_item(current, index);
    } else {
      // Cannot traverse further
      fprintf(stderr,
              "Error: Cannot set key part '%s' on a non-object/non-array.\n",
              parts[i]);
      free(key_copy);
      return 0;
    }
  }

  // Determine the value type and create the appropriate JSON value
  JsonValue *new_value = NULL;

  if (strcmp(value_str, "true") == 0) {
    new_value = create_json_value(JSON_BOOL);
    if (new_value)
      new_value->value.boolean = 1;
  } else if (strcmp(value_str, "false") == 0) {
    new_value = create_json_value(JSON_BOOL);
    if (new_value)
      new_value->value.boolean = 0;
  } else if (strcmp(value_str, "null") == 0) {
    new_value = create_json_value(JSON_NULL);
  } else {
    // Try to parse as number (but not if it's an empty string)
    char *endptr;
    double num = strtod(value_str, &endptr);
    if (*endptr == '\0' &&
        *value_str != '\0') { // Successfully parsed as a number and not empty
      new_value = create_json_value(JSON_NUMBER);
      if (new_value)
        new_value->value.number = num;
    } else { // Treat as string
      new_value = create_json_value(JSON_STRING);
      if (new_value)
        new_value->value.string = strdup(value_str);
    }
  }

  if (!new_value) {
    fprintf(stderr, "Error: Failed to create JSON value for '%s'.\n",
            value_str);
    free(key_copy);
    return 0;
  }

  // Add or replace the item in the parent object
  const char *last_key = parts[part_count - 1];
  int success = 0;

  if (current->type == JSON_OBJECT) {
    success = add_to_object(current, last_key, new_value);
  } else if (current->type == JSON_ARRAY) {
    char *endptr;
    long index = strtol(last_key, &endptr, 10);
    if (*endptr != '\0' || index < 0) {
      fprintf(stderr, "Error: Invalid array index '%s'.\n", last_key);
      free(key_copy);
      free_json_value(new_value);
      return 0;
    }

    // Extend array if needed
    while (index >= get_array_size(current)) {
      JsonValue *null_value = create_json_value(JSON_NULL);
      if (!null_value) {
        fprintf(stderr, "Error: Failed to create new array item.\n");
        free(key_copy);
        free_json_value(new_value);
        return 0;
      }
      add_to_array(current, null_value);
    }

    // Replace the item at the index
    JsonArrayItem *item = current->value.array_head;
    int i = 0;

    while (item && i < index) {
      item = item->next;
      i++;
    }

    if (item) {
      free_json_value(item->value);
      item->value = new_value;
      success = 1;
    } else {
      free_json_value(new_value);
      success = 0;
    }
  } else {
    fprintf(stderr, "Error: Cannot set key '%s' on a non-object/non-array.\n",
            last_key);
    free_json_value(new_value);
    success = 0;
  }

  free(key_copy);
  return success;
}

// Forward declaration for recursive printing
static void print_json_value(JsonValue *item, int indent);

/**
 * Prints indentation
 */
static void print_indent(int indent) {
  for (int i = 0; i < indent; i++) {
    printf("  ");
  }
}

// Helper: print a JSON-escaped string (without surrounding quotes)
static void print_escaped_string(const char *s) {
  if (!s)
    return;
  for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
    switch (*p) {
    case '"':
      putchar('\\');
      putchar('"');
      break;
    case '\\':
      putchar('\\');
      putchar('\\');
      break;
    case '\b':
      putchar('\\');
      putchar('b');
      break;
    case '\f':
      putchar('\\');
      putchar('f');
      break;
    case '\n':
      putchar('\\');
      putchar('n');
      break;
    case '\r':
      putchar('\\');
      putchar('r');
      break;
    case '\t':
      putchar('\\');
      putchar('t');
      break;
    default:
      putchar(*p);
      break;
    }
  }
}

/**
 * Recursively prints a JSON value with proper indentation
 */
static void print_json_value(JsonValue *item, int indent) {
  if (!item) {
    printf("null");
    return;
  }

  switch (item->type) {
  case JSON_NULL:
    printf("null");
    break;
  case JSON_BOOL:
    printf("%s", item->value.boolean ? "true" : "false");
    break;
  case JSON_NUMBER:
    // Check if it's an integer
    if (item->value.number == (int64_t)item->value.number) {
      printf("%" PRId64, (int64_t)item->value.number);
    } else {
      printf("%g", item->value.number);
    }
    break;
  case JSON_STRING:
    if (item->value.string) {
      putchar('"');
      print_escaped_string(item->value.string);
      putchar('"');
    } else {
      printf("\"\"");
    }
    break;
  case JSON_OBJECT: {
    if (!item->value.object_head) {
      printf("{}");
      break;
    }

    // Count the number of key-value pairs
    int count = 0;
    JsonKeyValue *kv = item->value.object_head;
    while (kv) {
      count++;
      kv = kv->next;
    }

    // Create an array of pointers to key-value pairs
    JsonKeyValue **kvs =
        (JsonKeyValue **)malloc(count * sizeof(JsonKeyValue *));
    if (!kvs) {
      fprintf(stderr,
              "Error: Memory allocation failed for sorting JSON keys.\n");
      printf("{...}"); // Fallback
      return;
    }

    // Fill the array
    kv = item->value.object_head;
    for (int i = 0; i < count; i++) {
      kvs[i] = kv;
      kv = kv->next;
    }

    // Sort the array alphabetically by key
    qsort(kvs, count, sizeof(JsonKeyValue *), compare_json_keys);

    // Print the sorted key-value pairs
    printf("{\n");
    int first = 1;

    for (int i = 0; i < count; i++) {
      if (!first) {
        printf(",\n");
      }

      print_indent(indent + 1);
      putchar('"');
      if (kvs[i]->key) {
        print_escaped_string(kvs[i]->key);
      }
      putchar('"');
      printf(": ");

      if (kvs[i]->value) {
        print_json_value(kvs[i]->value, indent + 1);
      } else {
        printf("null");
      }

      first = 0;
    }

    // Free the array
    free(kvs);

    printf("\n");
    print_indent(indent);
    printf("}");
    break;
  }
  case JSON_ARRAY: {
    if (!item->value.array_head) {
      printf("[]");
      break;
    }

    printf("[\n");
    JsonArrayItem *item_ptr = item->value.array_head;
    int first = 1;

    while (item_ptr) {
      if (!first) {
        printf(",\n");
      }

      print_indent(indent + 1);

      if (item_ptr->value) {
        print_json_value(item_ptr->value, indent + 1);
      } else {
        printf("null");
      }

      first = 0;
      item_ptr = item_ptr->next;
    }

    printf("\n");
    print_indent(indent);
    printf("]");
    break;
  }
  default:
    printf("(unknown type)");
    break;
  }
}

/**
 * Prints a JSON item appropriately
 *
 * @param item The JSON object to print
 */
void print_item(JsonValue *item) {
  if (!item) {
    printf("null\n");
    return;
  }

  // Special case for printing a single number value
  if (item->type == JSON_NUMBER) {
    // Check if it's an integer
    if (item->value.number == (int64_t)item->value.number) {
      int64_t int_value = (int64_t)item->value.number;
      printf("%" PRId64 "\n", int_value);
    } else {
      printf("%g\n", item->value.number);
    }
    return;
  }

  // Special case for printing a single string value
  if (item->type == JSON_STRING) {
    if (item->value.string) {
      printf("%s\n", item->value.string);
    } else {
      printf("\n");
    }
    return;
  }

  // Special case for printing a single boolean value
  if (item->type == JSON_BOOL) {
    printf("%s\n", item->value.boolean ? "true" : "false");
    return;
  }

  print_json_value(item, 0);
  printf("\n");
}
