/**
 * json_serialize.c - Implementation of JSON serialization functions
 */

#include "json_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

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

    // Prevent excessive recursion
    if (level > 1000) {
        fprintf(stderr, "Error: JSON nesting too deep\n");
        return 4; // Return "null" size
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
            char buffer[128]; // Larger buffer for safety
            int len = snprintf(buffer, sizeof(buffer), "%g", json->value.number);
            if (len < 0 || len >= (int)sizeof(buffer)) {
                // Handle error or truncation
                size = 16; // Use a safe default size
            } else {
                size = len;
            }
            break;
        }
        case JSON_STRING: {
            if (!json->value.string) {
                size = 2; // Just quotes for NULL string
            } else {
                char *escaped = escape_string(json->value.string);
                if (escaped) {
                    size_t escaped_len = strlen(escaped);
                    if (escaped_len > INT_MAX - 2) {
                        // Prevent integer overflow
                        fprintf(stderr, "Error: String too large to serialize\n");
                        free(escaped);
                        return 2; // Just quotes
                    }
                    size = (int)escaped_len + 2; // Add quotes
                    free(escaped);
                } else {
                    size = 2; // Just quotes
                }
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
                    int indent_size = 2 + level * 2; // Newline and indentation
                    if (indent_size < 0 || size > INT_MAX - indent_size) {
                        // Prevent integer overflow
                        fprintf(stderr, "Error: JSON too large to serialize\n");
                        return size;
                    }
                    size += indent_size;
                }

                int item_size = calculate_json_size(item->value, pretty, level + 1);
                if (item_size < 0 || size > INT_MAX - item_size) {
                    // Prevent integer overflow
                    fprintf(stderr, "Error: JSON too large to serialize\n");
                    return size;
                }
                size += item_size;

                first = 0;
                item = item->next;
            }

            if (pretty && json->value.array_head) {
                int indent_size = 1 + level * 2; // Newline and indentation for closing bracket
                if (indent_size < 0 || size > INT_MAX - indent_size) {
                    // Prevent integer overflow
                    fprintf(stderr, "Error: JSON too large to serialize\n");
                    return size;
                }
                size += indent_size;
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
                    int indent_size = 2 + level * 2; // Newline and indentation
                    if (indent_size < 0 || size > INT_MAX - indent_size) {
                        // Prevent integer overflow
                        fprintf(stderr, "Error: JSON too large to serialize\n");
                        return size;
                    }
                    size += indent_size;
                }

                if (!kv->key) {
                    size += 2; // Just quotes for NULL key
                } else {
                    char *escaped_key = escape_string(kv->key);
                    if (escaped_key) {
                        size_t escaped_len = strlen(escaped_key);
                        if (escaped_len > INT_MAX - 2 || size > INT_MAX - ((int)escaped_len + 2)) {
                            // Prevent integer overflow
                            fprintf(stderr, "Error: Key too large to serialize\n");
                            free(escaped_key);
                            return size;
                        }
                        size += (int)escaped_len + 2; // Add quotes
                        free(escaped_key);
                    } else {
                        size += 2; // Just quotes
                    }
                }

                size += 1; // Colon
                if (pretty) size += 1; // Space after colon

                int value_size = calculate_json_size(kv->value, pretty, level + 1);
                if (value_size < 0 || size > INT_MAX - value_size) {
                    // Prevent integer overflow
                    fprintf(stderr, "Error: JSON too large to serialize\n");
                    return size;
                }
                size += value_size;

                first = 0;
                kv = kv->next;
            }

            if (pretty && json->value.object_head) {
                int indent_size = 1 + level * 2; // Newline and indentation for closing brace
                if (indent_size < 0 || size > INT_MAX - indent_size) {
                    // Prevent integer overflow
                    fprintf(stderr, "Error: JSON too large to serialize\n");
                    return size;
                }
                size += indent_size;
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
        strncpy(buffer, "null", 5);
        return 4;
    }

    // Prevent excessive recursion
    if (level > 1000) {
        strncpy(buffer, "null", 5);
        return 4;
    }

    int pos = 0;

    switch (json->type) {
        case JSON_NULL:
            strncpy(buffer, "null", 5);
            pos = 4;
            break;
        case JSON_BOOL:
            if (json->value.boolean) {
                strncpy(buffer, "true", 5);
                pos = 4;
            } else {
                strncpy(buffer, "false", 6);
                pos = 5;
            }
            break;
        case JSON_NUMBER: {
            // Use snprintf for safety
            pos = snprintf(buffer, 128, "%g", json->value.number);
            if (pos < 0 || pos >= 128) {
                // Handle error or truncation
                strncpy(buffer, "0", 2);
                pos = 1;
            }
            break;
        }
        case JSON_STRING: {
            buffer[pos++] = '"';

            if (!json->value.string) {
                // Handle NULL string
                buffer[pos++] = '"';
                buffer[pos] = '\0';
            } else {
                char *escaped = escape_string(json->value.string);
                if (escaped) {
                    size_t escaped_len = strlen(escaped);
                    if (escaped_len < INT_MAX) {
                        strncpy(buffer + pos, escaped, escaped_len + 1);
                        pos += (int)escaped_len;
                    }
                    free(escaped);
                }

                buffer[pos++] = '"';
                buffer[pos] = '\0';
            }
            break;
        }
        case JSON_ARRAY: {
            buffer[pos++] = '[';
            buffer[pos] = '\0';

            JsonArrayItem *item = json->value.array_head;
            int first = 1;

            while (item) {
                if (!first) {
                    buffer[pos++] = ',';
                    if (pretty) buffer[pos++] = ' ';
                }

                if (pretty) {
                    buffer[pos++] = '\n';
                    int indent = (level + 1) * 2;
                    // Limit indentation to prevent buffer overflow
                    if (indent > 100) indent = 100;
                    for (int i = 0; i < indent; i++) {
                        buffer[pos++] = ' ';
                    }
                }

                int written = serialize_json_to_buffer(item->value, buffer + pos, pretty, level + 1);
                if (written < 0) {
                    // Handle error
                    return pos;
                }
                pos += written;

                first = 0;
                item = item->next;
            }

            if (pretty && json->value.array_head) {
                buffer[pos++] = '\n';
                int indent = level * 2;
                // Limit indentation to prevent buffer overflow
                if (indent > 100) indent = 100;
                for (int i = 0; i < indent; i++) {
                    buffer[pos++] = ' ';
                }
            }

            buffer[pos++] = ']';
            buffer[pos] = '\0';
            break;
        }
        case JSON_OBJECT: {
            buffer[pos++] = '{';
            buffer[pos] = '\0';

            JsonKeyValue *kv = json->value.object_head;
            int first = 1;

            while (kv) {
                if (!first) {
                    buffer[pos++] = ',';
                    if (pretty) buffer[pos++] = ' ';
                }

                if (pretty) {
                    buffer[pos++] = '\n';
                    int indent = (level + 1) * 2;
                    // Limit indentation to prevent buffer overflow
                    if (indent > 100) indent = 100;
                    for (int i = 0; i < indent; i++) {
                        buffer[pos++] = ' ';
                    }
                }

                buffer[pos++] = '"';

                if (!kv->key) {
                    // Handle NULL key
                    buffer[pos++] = '"';
                } else {
                    char *escaped_key = escape_string(kv->key);
                    if (escaped_key) {
                        size_t escaped_len = strlen(escaped_key);
                        if (escaped_len < INT_MAX) {
                            strncpy(buffer + pos, escaped_key, escaped_len + 1);
                            pos += (int)escaped_len;
                        }
                        free(escaped_key);
                    }

                    buffer[pos++] = '"';
                }

                buffer[pos++] = ':';
                if (pretty) buffer[pos++] = ' ';

                int written = serialize_json_to_buffer(kv->value, buffer + pos, pretty, level + 1);
                if (written < 0) {
                    // Handle error
                    return pos;
                }
                pos += written;

                first = 0;
                kv = kv->next;
            }

            if (pretty && json->value.object_head) {
                buffer[pos++] = '\n';
                int indent = level * 2;
                // Limit indentation to prevent buffer overflow
                if (indent > 100) indent = 100;
                for (int i = 0; i < indent; i++) {
                    buffer[pos++] = ' ';
                }
            }

            buffer[pos++] = '}';
            buffer[pos] = '\0';
            break;
        }
    }

    // Ensure null termination
    buffer[pos] = '\0';
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

    // Validate the calculated size
    if (size <= 0) {
        fprintf(stderr, "Error: Invalid size calculated for JSON string\n");
        return strdup("null");
    }

    // Use a reasonable maximum size to prevent allocation issues
    if (size > 100 * 1024 * 1024) { // 100 MB limit
        fprintf(stderr, "Error: JSON string too large (over 100MB)\n");
        return strdup("null");
    }

    // Allocate memory for the JSON string with extra padding for safety
    char *str = (char *)malloc((size_t)size + 16); // Add extra padding
    if (!str) {
        fprintf(stderr, "Error: Memory allocation failed for JSON string\n");
        return NULL;
    }

    // Serialize the JSON value to the buffer
    int written = serialize_json_to_buffer(json, str, pretty, 0);

    // Ensure proper null termination
    if (written >= 0 && written <= size + 15) {
        str[written] = '\0';
    } else {
        // Safety measure if serialization wrote more than expected
        str[size + 15] = '\0';
    }

    return str;
}
