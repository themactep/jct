/**
 * json_config.c - Implementation of JSON configuration manipulation functions
 */

#include "json_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

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

    if (!kv_a->key && !kv_b->key) return 0;
    if (!kv_a->key) return -1;
    if (!kv_b->key) return 1;

    return strcmp(kv_a->key, kv_b->key);
}

/**
 * Writes a JSON value directly to a file with proper indentation
 */
static int write_json_to_file(FILE *file, JsonValue *json, int indent) {
    if (!file || !json) {
        return 0;
    }

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
                        case '\\': success = success && (fprintf(file, "\\\\") > 0); break;
                        case '\"': success = success && (fprintf(file, "\\\"") > 0); break;
                        case '\b': success = success && (fprintf(file, "\\b") > 0); break;
                        case '\f': success = success && (fprintf(file, "\\f") > 0); break;
                        case '\n': success = success && (fprintf(file, "\\n") > 0); break;
                        case '\r': success = success && (fprintf(file, "\\r") > 0); break;
                        case '\t': success = success && (fprintf(file, "\\t") > 0); break;
                        default:
                            if ((unsigned char)*p < 32) {
                                success = success && (fprintf(file, "\\u%04x", (unsigned char)*p) > 0);
                            } else {
                                success = success && (fprintf(file, "%c", *p) > 0);
                            }
                            break;
                    }
                    if (!success) break;
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
            JsonKeyValue **kvs = (JsonKeyValue **)malloc(count * sizeof(JsonKeyValue *));
            if (!kvs) {
                fprintf(stderr, "Error: Memory allocation failed for sorting JSON keys.\n");
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

                // Print key
                success = success && (fprintf(file, "\"%s\": ", kvs[i]->key ? kvs[i]->key : "") > 0);

                // Print value
                if (kvs[i]->value) {
                    success = success && write_json_to_file(file, kvs[i]->value, indent + 1);
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
    if (!json) {
        return 0;
    }

    FILE *file = fopen(filepath, "w");
    if (!file) {
        fprintf(stderr, "Error: Failed to open file '%s' for writing: %s\n",
                filepath, strerror(errno));
        return 0;
    }

    int success = write_json_to_file(file, json, 0);

    // Add a final newline
    if (success) {
        success = (fprintf(file, "\n") > 0);
    }

    fclose(file);

    if (!success) {
        fprintf(stderr, "Error: Failed to write complete JSON to '%s'.\n", filepath);
        return 0;
    }

    return 1;
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
                fprintf(stderr, "Error: Invalid array index '%s' for key '%s'.\n", token, key);
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
                    fprintf(stderr, "Error: Failed to create intermediate object for key '%s'.\n", parts[i]);
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
                fprintf(stderr, "Error: Invalid array index '%s' for key '%s'.\n", parts[i], key);
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
            fprintf(stderr, "Error: Cannot set key part '%s' on a non-object/non-array.\n", parts[i]);
            free(key_copy);
            return 0;
        }
    }

    // Determine the value type and create the appropriate JSON value
    JsonValue *new_value = NULL;

    if (strcmp(value_str, "true") == 0) {
        new_value = create_json_value(JSON_BOOL);
        if (new_value) new_value->value.boolean = 1;
    } else if (strcmp(value_str, "false") == 0) {
        new_value = create_json_value(JSON_BOOL);
        if (new_value) new_value->value.boolean = 0;
    } else if (strcmp(value_str, "null") == 0) {
        new_value = create_json_value(JSON_NULL);
    } else {
        // Try to parse as number
        char *endptr;
        double num = strtod(value_str, &endptr);
        if (*endptr == '\0') { // Successfully parsed as a number
            new_value = create_json_value(JSON_NUMBER);
            if (new_value) new_value->value.number = num;
        } else { // Treat as string
            new_value = create_json_value(JSON_STRING);
            if (new_value) new_value->value.string = strdup(value_str);
        }
    }

    if (!new_value) {
        fprintf(stderr, "Error: Failed to create JSON value for '%s'.\n", value_str);
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
        fprintf(stderr, "Error: Cannot set key '%s' on a non-object/non-array.\n", last_key);
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
                printf("\"%s\"", item->value.string);
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
            JsonKeyValue **kvs = (JsonKeyValue **)malloc(count * sizeof(JsonKeyValue *));
            if (!kvs) {
                fprintf(stderr, "Error: Memory allocation failed for sorting JSON keys.\n");
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
                printf("\"%s\": ", kvs[i]->key ? kvs[i]->key : "");

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

    print_json_value(item, 0);
    printf("\n");
}
