/**
 * json_serialize.c - Implementation of JSON serialization functions
 */

#include "json_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Function prototypes for internal use
static char *escape_string(const char *str);
static int calculate_json_size(JsonValue *json, int pretty, int level);
static int serialize_json_to_buffer(JsonValue *json, char *buffer, int pretty, int level);

/**
 * Escapes a string for JSON output
 */
static char *escape_string(const char *str) {
    if (!str) {
        return NULL;
    }
    
    // Count the number of characters that need escaping
    size_t len = strlen(str);
    size_t escaped_len = len;
    
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (c == '"' || c == '\\' || c == '\b' || c == '\f' || c == '\n' || c == '\r' || c == '\t') {
            escaped_len++;
        }
    }
    
    // Allocate memory for the escaped string
    char *escaped = (char *)malloc(escaped_len + 1);
    if (!escaped) {
        return NULL;
    }
    
    // Copy the string with escaping
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        
        switch (c) {
            case '"':
                escaped[j++] = '\\';
                escaped[j++] = '"';
                break;
            case '\\':
                escaped[j++] = '\\';
                escaped[j++] = '\\';
                break;
            case '\b':
                escaped[j++] = '\\';
                escaped[j++] = 'b';
                break;
            case '\f':
                escaped[j++] = '\\';
                escaped[j++] = 'f';
                break;
            case '\n':
                escaped[j++] = '\\';
                escaped[j++] = 'n';
                break;
            case '\r':
                escaped[j++] = '\\';
                escaped[j++] = 'r';
                break;
            case '\t':
                escaped[j++] = '\\';
                escaped[j++] = 't';
                break;
            default:
                escaped[j++] = c;
                break;
        }
    }
    
    escaped[j] = '\0';
    return escaped;
}

/**
 * Calculates the size needed for the JSON string
 */
static int calculate_json_size(JsonValue *json, int pretty, int level) {
    if (!json) {
        return 4; // "null"
    }
    
    int size = 0;
    
    switch (json->type) {
        case JSON_NULL:
            size = 4; // "null"
            break;
        case JSON_BOOL:
            size = json->value.boolean ? 4 : 5; // "true" or "false"
            break;
        case JSON_NUMBER: {
            char buffer[64];
            size = snprintf(buffer, sizeof(buffer), "%g", json->value.number);
            break;
        }
        case JSON_STRING: {
            char *escaped = escape_string(json->value.string);
            if (escaped) {
                size = strlen(escaped) + 2; // Add quotes
                free(escaped);
            } else {
                size = 2; // Just quotes
            }
            break;
        }
        case JSON_ARRAY: {
            size = 2; // []
            
            JsonArrayItem *item = json->value.array_head;
            int first = 1;
            
            while (item) {
                if (!first) {
                    size += 1; // Comma
                    if (pretty) size += 1; // Space after comma
                }
                
                if (pretty) {
                    size += 2 + level * 2; // Newline and indentation
                }
                
                size += calculate_json_size(item->value, pretty, level + 1);
                
                first = 0;
                item = item->next;
            }
            
            if (pretty && json->value.array_head) {
                size += 1 + level * 2; // Newline and indentation for closing bracket
            }
            
            break;
        }
        case JSON_OBJECT: {
            size = 2; // {}
            
            JsonKeyValue *kv = json->value.object_head;
            int first = 1;
            
            while (kv) {
                if (!first) {
                    size += 1; // Comma
                    if (pretty) size += 1; // Space after comma
                }
                
                if (pretty) {
                    size += 2 + level * 2; // Newline and indentation
                }
                
                char *escaped_key = escape_string(kv->key);
                if (escaped_key) {
                    size += strlen(escaped_key) + 2; // Add quotes
                    free(escaped_key);
                } else {
                    size += 2; // Just quotes
                }
                
                size += 1; // Colon
                if (pretty) size += 1; // Space after colon
                
                size += calculate_json_size(kv->value, pretty, level + 1);
                
                first = 0;
                kv = kv->next;
            }
            
            if (pretty && json->value.object_head) {
                size += 1 + level * 2; // Newline and indentation for closing brace
            }
            
            break;
        }
    }
    
    return size;
}

/**
 * Serializes a JSON value to a buffer
 */
static int serialize_json_to_buffer(JsonValue *json, char *buffer, int pretty, int level) {
    if (!json || !buffer) {
        strcpy(buffer, "null");
        return 4;
    }
    
    int pos = 0;
    
    switch (json->type) {
        case JSON_NULL:
            strcpy(buffer, "null");
            pos = 4;
            break;
        case JSON_BOOL:
            if (json->value.boolean) {
                strcpy(buffer, "true");
                pos = 4;
            } else {
                strcpy(buffer, "false");
                pos = 5;
            }
            break;
        case JSON_NUMBER:
            pos = sprintf(buffer, "%g", json->value.number);
            break;
        case JSON_STRING: {
            buffer[pos++] = '"';
            
            char *escaped = escape_string(json->value.string);
            if (escaped) {
                strcpy(buffer + pos, escaped);
                pos += strlen(escaped);
                free(escaped);
            }
            
            buffer[pos++] = '"';
            buffer[pos] = '\0';
            break;
        }
        case JSON_ARRAY: {
            buffer[pos++] = '[';
            
            JsonArrayItem *item = json->value.array_head;
            int first = 1;
            
            while (item) {
                if (!first) {
                    buffer[pos++] = ',';
                    if (pretty) buffer[pos++] = ' ';
                }
                
                if (pretty) {
                    buffer[pos++] = '\n';
                    for (int i = 0; i < (level + 1) * 2; i++) {
                        buffer[pos++] = ' ';
                    }
                }
                
                pos += serialize_json_to_buffer(item->value, buffer + pos, pretty, level + 1);
                
                first = 0;
                item = item->next;
            }
            
            if (pretty && json->value.array_head) {
                buffer[pos++] = '\n';
                for (int i = 0; i < level * 2; i++) {
                    buffer[pos++] = ' ';
                }
            }
            
            buffer[pos++] = ']';
            buffer[pos] = '\0';
            break;
        }
        case JSON_OBJECT: {
            buffer[pos++] = '{';
            
            JsonKeyValue *kv = json->value.object_head;
            int first = 1;
            
            while (kv) {
                if (!first) {
                    buffer[pos++] = ',';
                    if (pretty) buffer[pos++] = ' ';
                }
                
                if (pretty) {
                    buffer[pos++] = '\n';
                    for (int i = 0; i < (level + 1) * 2; i++) {
                        buffer[pos++] = ' ';
                    }
                }
                
                buffer[pos++] = '"';
                
                char *escaped_key = escape_string(kv->key);
                if (escaped_key) {
                    strcpy(buffer + pos, escaped_key);
                    pos += strlen(escaped_key);
                    free(escaped_key);
                }
                
                buffer[pos++] = '"';
                buffer[pos++] = ':';
                if (pretty) buffer[pos++] = ' ';
                
                pos += serialize_json_to_buffer(kv->value, buffer + pos, pretty, level + 1);
                
                first = 0;
                kv = kv->next;
            }
            
            if (pretty && json->value.object_head) {
                buffer[pos++] = '\n';
                for (int i = 0; i < level * 2; i++) {
                    buffer[pos++] = ' ';
                }
            }
            
            buffer[pos++] = '}';
            buffer[pos] = '\0';
            break;
        }
    }
    
    return pos;
}

/**
 * Converts a JSON value to a string
 * 
 * @param json The JSON value to convert
 * @param pretty Whether to format the output with indentation
 * @return A newly allocated string containing the JSON representation
 */
char *json_to_string(JsonValue *json, int pretty) {
    if (!json) {
        char *str = strdup("null");
        return str;
    }
    
    // Calculate the size needed for the JSON string
    int size = calculate_json_size(json, pretty, 0);
    
    // Allocate memory for the JSON string
    char *str = (char *)malloc(size + 1);
    if (!str) {
        return NULL;
    }
    
    // Serialize the JSON value to the buffer
    serialize_json_to_buffer(json, str, pretty, 0);
    
    return str;
}
