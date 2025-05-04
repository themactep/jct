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
    
    char *json_str = json_to_string(json, 1); // Pretty print
    if (!json_str) {
        return 0;
    }
    
    FILE *file = fopen(filepath, "w");
    if (!file) {
        fprintf(stderr, "Error: Failed to open file '%s' for writing: %s\n", 
                filepath, strerror(errno));
        free(json_str);
        return 0;
    }
    
    size_t len = strlen(json_str);
    size_t written = fwrite(json_str, 1, len, file);
    fclose(file);
    free(json_str);
    
    if (written != len) {
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

    switch (item->type) {
        case JSON_NULL:
            printf("null\n");
            break;
        case JSON_BOOL:
            printf("%s\n", item->value.boolean ? "true" : "false");
            break;
        case JSON_NUMBER:
            // Check if it's an integer
            if (item->value.number == (int64_t)item->value.number) {
                printf("%" PRId64 "\n", (int64_t)item->value.number);
            } else {
                printf("%g\n", item->value.number);
            }
            break;
        case JSON_STRING:
            printf("%s\n", item->value.string);
            break;
        case JSON_OBJECT:
        case JSON_ARRAY: {
            char *json_str = json_to_string(item, 1); // Pretty print
            if (json_str) {
                printf("%s\n", json_str);
                free(json_str);
            } else {
                printf("Error: Failed to convert to JSON string\n");
            }
            break;
        }
        default:
            fprintf(stderr, "Error: Unknown JSON type.\n");
    }
}
