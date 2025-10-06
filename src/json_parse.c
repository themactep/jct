/**
 * json_parse.c - Implementation of JSON parsing functions
 */

#include "json_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

// Simple JSON parser state
typedef struct {
    const char *json;
    size_t pos;
    size_t len;
} JsonParser;

// Function prototypes for internal use
static void skip_whitespace(JsonParser *parser);
static char *parse_string(JsonParser *parser);
static JsonValue *parse_value(JsonParser *parser);
static JsonValue *parse_array(JsonParser *parser);
static JsonValue *parse_object(JsonParser *parser);
static JsonValue *parse_number(JsonParser *parser);

// Function to skip whitespace
static void skip_whitespace(JsonParser *parser) {
    while (parser->pos < parser->len &&
           (parser->json[parser->pos] == ' ' ||
            parser->json[parser->pos] == '\t' ||
            parser->json[parser->pos] == '\n' ||
            parser->json[parser->pos] == '\r')) {
        parser->pos++;
    }
}

// Function to parse a JSON string
static char *parse_string(JsonParser *parser) {
    if (parser->pos >= parser->len || parser->json[parser->pos] != '"') {
        return NULL;
    }

    parser->pos++; // Skip opening quote

    // Find the closing quote and count actual characters needed
    int escaped = 0;
    size_t actual_len = 0;

    // First pass: find the end and count unescaped length
    size_t temp_pos = parser->pos;
    while (temp_pos < parser->len) {
        char c = parser->json[temp_pos];

        if (escaped) {
            escaped = 0;
            actual_len++; // Each escape sequence becomes one character
        } else if (c == '\\') {
            escaped = 1;
        } else if (c == '"') {
            break;
        } else {
            actual_len++;
        }

        temp_pos++;
    }

    if (temp_pos >= parser->len) {
        return NULL; // Unterminated string
    }

    // Allocate memory for the unescaped string
    char *str = (char *)malloc(actual_len + 1);
    if (!str) {
        return NULL;
    }

    // Second pass: copy and unescape the string
    size_t j = 0;
    escaped = 0;
    while (parser->pos < parser->len) {
        char c = parser->json[parser->pos];

        if (escaped) {
            // Handle escape sequences
            switch (c) {
                case '"':
                    str[j++] = '"';
                    break;
                case '\\':
                    str[j++] = '\\';
                    break;
                case 'b':
                    str[j++] = '\b';
                    break;
                case 'f':
                    str[j++] = '\f';
                    break;
                case 'n':
                    str[j++] = '\n';
                    break;
                case 'r':
                    str[j++] = '\r';
                    break;
                case 't':
                    str[j++] = '\t';
                    break;
                case '/':
                    str[j++] = '/';
                    break;
                default:
                    // For unrecognized escape sequences, keep the character as-is
                    str[j++] = c;
                    break;
            }
            escaped = 0;
        } else if (c == '\\') {
            escaped = 1;
        } else if (c == '"') {
            break;
        } else {
            str[j++] = c;
        }

        parser->pos++;
    }

    parser->pos++; // Skip closing quote
    str[j] = '\0';

    return str;
}

// Function to parse a JSON array
static JsonValue *parse_array(JsonParser *parser) {
    if (parser->pos >= parser->len || parser->json[parser->pos] != '[') {
        return NULL;
    }

    parser->pos++; // Skip opening bracket
    skip_whitespace(parser);

    JsonValue *array = create_json_value(JSON_ARRAY);
    if (!array) {
        return NULL;
    }

    // Check for empty array
    if (parser->pos < parser->len && parser->json[parser->pos] == ']') {
        parser->pos++; // Skip closing bracket
        return array;
    }

    // Parse array elements
    while (parser->pos < parser->len) {
        skip_whitespace(parser);

        JsonValue *value = parse_value(parser);
        if (!value) {
            free_json_value(array);
            return NULL;
        }

        if (!add_to_array(array, value)) {
            free_json_value(value);
            free_json_value(array);
            return NULL;
        }

        skip_whitespace(parser);

        if (parser->pos < parser->len && parser->json[parser->pos] == ']') {
            parser->pos++; // Skip closing bracket
            return array;
        }

        if (parser->pos < parser->len && parser->json[parser->pos] == ',') {
            parser->pos++; // Skip comma
        } else {
            free_json_value(array);
            return NULL; // Expected comma or closing bracket
        }
    }

    free_json_value(array);
    return NULL; // Unterminated array
}

// Function to parse a JSON object
static JsonValue *parse_object(JsonParser *parser) {
    if (parser->pos >= parser->len || parser->json[parser->pos] != '{') {
        return NULL;
    }

    parser->pos++; // Skip opening brace
    skip_whitespace(parser);

    JsonValue *object = create_json_value(JSON_OBJECT);
    if (!object) {
        return NULL;
    }

    // Check for empty object
    if (parser->pos < parser->len && parser->json[parser->pos] == '}') {
        parser->pos++; // Skip closing brace
        return object;
    }

    // Parse object key-value pairs
    while (parser->pos < parser->len) {
        skip_whitespace(parser);

        // Parse key
        char *key = parse_string(parser);
        if (!key) {
            free_json_value(object);
            return NULL;
        }

        skip_whitespace(parser);

        // Check for colon
        if (parser->pos >= parser->len || parser->json[parser->pos] != ':') {
            free(key);
            free_json_value(object);
            return NULL;
        }

        parser->pos++; // Skip colon
        skip_whitespace(parser);

        // Parse value
        JsonValue *value = parse_value(parser);
        if (!value) {
            free(key);
            free_json_value(object);
            return NULL;
        }

        // Add key-value pair to object
        if (!add_to_object(object, key, value)) {
            free(key);
            free_json_value(value);
            free_json_value(object);
            return NULL;
        }

        free(key); // Key is copied in add_to_object

        skip_whitespace(parser);

        if (parser->pos < parser->len && parser->json[parser->pos] == '}') {
            parser->pos++; // Skip closing brace
            return object;
        }

        if (parser->pos < parser->len && parser->json[parser->pos] == ',') {
            parser->pos++; // Skip comma
        } else {
            free_json_value(object);
            return NULL; // Expected comma or closing brace
        }
    }

    free_json_value(object);
    return NULL; // Unterminated object
}

// Function to parse a JSON number
static JsonValue *parse_number(JsonParser *parser) {
    if (parser->pos >= parser->len) {
        return NULL;
    }

    // Check if it's a number
    char c = parser->json[parser->pos];
    if (!isdigit(c) && c != '-' && c != '+' && c != '.') {
        return NULL;
    }

    // Find the end of the number
    size_t start = parser->pos;
    int has_decimal = 0;
    int has_exponent = 0;

    while (parser->pos < parser->len) {
        c = parser->json[parser->pos];

        if (c == '.') {
            if (has_decimal) {
                break; // Multiple decimal points not allowed
            }
            has_decimal = 1;
        } else if (c == 'e' || c == 'E') {
            if (has_exponent) {
                break; // Multiple exponents not allowed
            }
            has_exponent = 1;
        } else if (isdigit(c) || c == '-' || c == '+') {
            // Valid number character
        } else {
            break; // End of number
        }

        parser->pos++;
    }

    // Extract the number string
    size_t len = parser->pos - start;
    char *num_str = (char *)malloc(len + 1);
    if (!num_str) {
        return NULL;
    }

    memcpy(num_str, parser->json + start, len);
    num_str[len] = '\0';

    // Convert to number
    double num = strtod(num_str, NULL);
    free(num_str);

    // Create JSON number value
    JsonValue *value = create_json_value(JSON_NUMBER);
    if (!value) {
        return NULL;
    }

    value->value.number = num;
    return value;
}

// Function to parse a JSON value
static JsonValue *parse_value(JsonParser *parser) {
    if (parser->pos >= parser->len) {
        return NULL;
    }

    skip_whitespace(parser);

    char c = parser->json[parser->pos];

    switch (c) {
        case '{':
            return parse_object(parser);
        case '[':
            return parse_array(parser);
        case '"': {
            char *str = parse_string(parser);
            if (!str) {
                return NULL;
            }

            JsonValue *value = create_json_value(JSON_STRING);
            if (!value) {
                free(str);
                return NULL;
            }

            value->value.string = str;
            return value;
        }
        case 't':
            if (parser->pos + 3 < parser->len &&
                parser->json[parser->pos + 1] == 'r' &&
                parser->json[parser->pos + 2] == 'u' &&
                parser->json[parser->pos + 3] == 'e') {

                parser->pos += 4;

                JsonValue *value = create_json_value(JSON_BOOL);
                if (!value) {
                    return NULL;
                }

                value->value.boolean = 1;
                return value;
            }
            return NULL;
        case 'f':
            if (parser->pos + 4 < parser->len &&
                parser->json[parser->pos + 1] == 'a' &&
                parser->json[parser->pos + 2] == 'l' &&
                parser->json[parser->pos + 3] == 's' &&
                parser->json[parser->pos + 4] == 'e') {

                parser->pos += 5;

                JsonValue *value = create_json_value(JSON_BOOL);
                if (!value) {
                    return NULL;
                }

                value->value.boolean = 0;
                return value;
            }
            return NULL;
        case 'n':
            if (parser->pos + 3 < parser->len &&
                parser->json[parser->pos + 1] == 'u' &&
                parser->json[parser->pos + 2] == 'l' &&
                parser->json[parser->pos + 3] == 'l') {

                parser->pos += 4;

                JsonValue *value = create_json_value(JSON_NULL);
                if (!value) {
                    return NULL;
                }

                return value;
            }
            return NULL;
        default:
            return parse_number(parser);
    }
}

/**
 * Parse JSON from a string
 */
JsonValue *parse_json_string(const char *json_str) {
    if (!json_str) {
        fprintf(stderr, "Error: NULL JSON string provided\n");
        return NULL;
    }

    size_t len = strlen(json_str);
    if (len == 0) {
        fprintf(stderr, "Error: Empty JSON string provided\n");
        return NULL;
    }

    // Check for reasonable string size (100MB limit)
    if (len > 100 * 1024 * 1024) {
        fprintf(stderr, "Error: JSON string too large (over 100MB)\n");
        return NULL;
    }

    JsonParser parser = {
        .json = json_str,
        .pos = 0,
        .len = len
    };

    JsonValue *result = parse_value(&parser);

    // Check if the entire string was parsed
    skip_whitespace(&parser);
    if (parser.pos < parser.len) {
        fprintf(stderr, "Warning: Extra characters found after JSON data\n");
    }

    return result;
}

/**
 * Parse JSON from a file
 */
JsonValue *parse_json_file(const char *filepath) {
    FILE *file = fopen(filepath, "r");
    if (!file) {
        fprintf(stderr, "Error: Failed to open file '%s': %s\n",
                filepath, strerror(errno));
        return NULL;
    }

    // Get file size
    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "Error: Failed to seek to end of file '%s': %s\n",
                filepath, strerror(errno));
        fclose(file);
        return NULL;
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        fprintf(stderr, "Error: Failed to get file size for '%s': %s\n",
                filepath, strerror(errno));
        fclose(file);
        return NULL;
    }

    // Check for empty file
    if (file_size == 0) {
        fprintf(stderr, "Error: File '%s' is empty\n", filepath);
        fclose(file);
        // Return an empty object instead of NULL for empty files
        return create_json_value(JSON_OBJECT);
    }

    // Check for reasonable file size (100MB limit)
    if (file_size > 100 * 1024 * 1024) {
        fprintf(stderr, "Error: File '%s' is too large (over 100MB)\n", filepath);
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Error: Failed to seek to start of file '%s': %s\n",
                filepath, strerror(errno));
        fclose(file);
        return NULL;
    }

    // Read file content
    char *buffer = (char*)malloc((size_t)file_size + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed for file content (size: %ld).\n", file_size);
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, (size_t)file_size, file);
    if (read_size == 0) {
        fprintf(stderr, "Error: Failed to read from file '%s': %s\n",
                filepath, strerror(errno));
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[read_size] = '\0';
    fclose(file);

    // Parse JSON
    JsonValue *json = parse_json_string(buffer);
    free(buffer);

    if (!json) {
        fprintf(stderr, "Error: Failed to parse JSON in '%s'.\n", filepath);
        // Return an empty object instead of NULL for parse failures
        return create_json_value(JSON_OBJECT);
    }

    return json;
}
