/**
 * json_value.c - Implementation of JSON value handling functions
 */

#include "json_config.h"
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
