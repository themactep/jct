/**
 * json_parse.c - Implementation of JSON parsing functions
 */

#include "json_config.h"
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static int hex_digit_value(char c);
static bool parse_unicode_code_unit(JsonParser *parser, unsigned int *codepoint);
static bool parse_unicode_escape(JsonParser *parser, unsigned int *codepoint);
static bool append_utf8(char *str, size_t *len, unsigned int codepoint);

// Function to skip whitespace
static void skip_whitespace(JsonParser *parser) {
  while (parser->pos < parser->len && (parser->json[parser->pos] == ' ' ||
                                       parser->json[parser->pos] == '\t' ||
                                       parser->json[parser->pos] == '\n' ||
                                       parser->json[parser->pos] == '\r')) {
    parser->pos++;
  }
}

static int hex_digit_value(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }

  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }

  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }

  return -1;
}

static bool append_utf8(char *str, size_t *len, unsigned int codepoint) {
  if (codepoint <= 0x7F) {
    str[(*len)++] = (char)codepoint;
    return true;
  }

  if (codepoint <= 0x7FF) {
    str[(*len)++] = (char)(0xC0 | (codepoint >> 6));
    str[(*len)++] = (char)(0x80 | (codepoint & 0x3F));
    return true;
  }

  if (codepoint <= 0xFFFF) {
    str[(*len)++] = (char)(0xE0 | (codepoint >> 12));
    str[(*len)++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    str[(*len)++] = (char)(0x80 | (codepoint & 0x3F));
    return true;
  }

  if (codepoint <= 0x10FFFF) {
    str[(*len)++] = (char)(0xF0 | (codepoint >> 18));
    str[(*len)++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
    str[(*len)++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    str[(*len)++] = (char)(0x80 | (codepoint & 0x3F));
    return true;
  }

  return false;
}

static bool parse_unicode_code_unit(JsonParser *parser, unsigned int *codepoint) {
  unsigned int cp = 0;

  if (parser->pos >= parser->len || parser->json[parser->pos] != 'u') {
    return false;
  }

  if (parser->pos + 4 >= parser->len) {
    return false;
  }

  for (size_t i = 1; i <= 4; i++) {
    int value = hex_digit_value(parser->json[parser->pos + i]);
    if (value < 0) {
      return false;
    }

    cp = (cp << 4) | (unsigned int)value;
  }

  parser->pos += 5;
  *codepoint = cp;
  return true;
}

static bool parse_unicode_escape(JsonParser *parser, unsigned int *codepoint) {
  unsigned int cp = 0;
  unsigned int low = 0;

  if (!parse_unicode_code_unit(parser, &cp)) {
    return false;
  }

  if (cp >= 0xD800 && cp <= 0xDBFF) {
    unsigned int high = cp;

    if (parser->pos + 1 >= parser->len || parser->json[parser->pos] != '\\' ||
        parser->json[parser->pos + 1] != 'u') {
      return false;
    }

    parser->pos++;
    if (!parse_unicode_code_unit(parser, &low) || low < 0xDC00 ||
        low > 0xDFFF) {
      return false;
    }

    cp = 0x10000 + (((high - 0xD800) << 10) | (low - 0xDC00));
  } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
    return false;
  }

  *codepoint = cp;
  return true;
}

// Function to parse a JSON string
static char *parse_string(JsonParser *parser) {
  if (parser->pos >= parser->len || parser->json[parser->pos] != '"') {
    return NULL;
  }

  parser->pos++; // Skip opening quote

  /*
   * The decoded string can never be longer than the remaining byte count in
   * the input, so allocate that upper bound and shrink before returning.
   */
  char *str = (char *)malloc((parser->len - parser->pos) + 1);
  if (!str) {
    return NULL;
  }

  size_t j = 0;
  while (parser->pos < parser->len) {
    unsigned char c = (unsigned char)parser->json[parser->pos];

    if (c == '"') {
      parser->pos++;
      str[j] = '\0';

      char *shrunk = (char *)realloc(str, j + 1);
      return shrunk ? shrunk : str;
    }

    if (c == '\\') {
      unsigned int codepoint;

      parser->pos++;
      if (parser->pos >= parser->len) {
        free(str);
        return NULL;
      }

      c = (unsigned char)parser->json[parser->pos];
      switch (c) {
      case '"':
        str[j++] = '"';
        parser->pos++;
        continue;
      case '\\':
        str[j++] = '\\';
        parser->pos++;
        continue;
      case '/':
        str[j++] = '/';
        parser->pos++;
        continue;
      case 'b':
        str[j++] = '\b';
        parser->pos++;
        continue;
      case 'f':
        str[j++] = '\f';
        parser->pos++;
        continue;
      case 'n':
        str[j++] = '\n';
        parser->pos++;
        continue;
      case 'r':
        str[j++] = '\r';
        parser->pos++;
        continue;
      case 't':
        str[j++] = '\t';
        parser->pos++;
        continue;
      case 'u':
        if (!parse_unicode_escape(parser, &codepoint) ||
            !append_utf8(str, &j, codepoint)) {
          free(str);
          return NULL;
        }
        continue;
      default:
        free(str);
        return NULL;
      }
    }

    if (c < 0x20) {
      free(str);
      return NULL;
    }

    str[j++] = (char)c;
    parser->pos++;
  }

  free(str);
  return NULL;
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
  bool has_fraction = false;
  bool has_exponent = false;
  bool overflow = false;
  char *endptr = NULL;
  JsonValue *value = NULL;
  size_t token_start = parser->pos;

  if (parser->pos >= parser->len) {
    return NULL;
  }

  if (parser->json[parser->pos] == '-') {
    parser->pos++;
    if (parser->pos >= parser->len) {
      return NULL;
    }
  }

  if (!isdigit((unsigned char)parser->json[parser->pos])) {
    return NULL;
  }

  if (parser->json[parser->pos] == '0') {
    parser->pos++;
  } else {
    while (parser->pos < parser->len &&
           isdigit((unsigned char)parser->json[parser->pos])) {
      parser->pos++;
    }
  }

  if (parser->pos < parser->len && parser->json[parser->pos] == '.') {
    has_fraction = true;
    parser->pos++;
    if (parser->pos >= parser->len ||
        !isdigit((unsigned char)parser->json[parser->pos])) {
      return NULL;
    }

    while (parser->pos < parser->len &&
           isdigit((unsigned char)parser->json[parser->pos])) {
      parser->pos++;
    }
  }

  if (parser->pos < parser->len &&
      (parser->json[parser->pos] == 'e' || parser->json[parser->pos] == 'E')) {
    has_exponent = true;
    parser->pos++;
    if (parser->pos < parser->len &&
        (parser->json[parser->pos] == '+' || parser->json[parser->pos] == '-')) {
      parser->pos++;
    }
    if (parser->pos >= parser->len ||
        !isdigit((unsigned char)parser->json[parser->pos])) {
      return NULL;
    }

    while (parser->pos < parser->len &&
           isdigit((unsigned char)parser->json[parser->pos])) {
      parser->pos++;
    }
  }

  size_t len = parser->pos - token_start;

  char *num_str = (char *)malloc(len + 1);
  if (!num_str) {
    return NULL;
  }

  memcpy(num_str, parser->json + token_start, len);
  num_str[len] = '\0';

  if (!has_fraction && !has_exponent) {
    errno = 0;
    int64_t integer = strtoll(num_str, &endptr, 10);
    if (errno == ERANGE) {
      overflow = true;
    } else if (endptr && *endptr == '\0') {
      free(num_str);
      return create_json_integer_value(integer);
    }
  }

  errno = 0;
  double num = strtod(num_str, &endptr);
  if ((errno == ERANGE && overflow) || !endptr || *endptr != '\0') {
    free(num_str);
    return NULL;
  }
  free(num_str);

  value = create_json_double_value(num);
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
    if (parser->pos + 3 < parser->len && parser->json[parser->pos + 1] == 'r' &&
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
    if (parser->pos + 4 < parser->len && parser->json[parser->pos + 1] == 'a' &&
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
    if (parser->pos + 3 < parser->len && parser->json[parser->pos + 1] == 'u' &&
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

  JsonParser parser = {.json = json_str, .pos = 0, .len = len};

  JsonValue *result = parse_value(&parser);

  // Check if the entire string was parsed
  skip_whitespace(&parser);
  if (parser.pos < parser.len) {
    fprintf(stderr, "Error: Extra characters found after JSON data\n");
    free_json_value(result);
    return NULL;
  }

  return result;
}

/**
 * Parse JSON from a file
 */
JsonValue *parse_json_file(const char *filepath) {
  FILE *file = fopen(filepath, "r");
  if (!file) {
    fprintf(stderr, "Error: Failed to open file '%s': %s\n", filepath,
            strerror(errno));
    return NULL;
  }

  // Get file size
  if (fseek(file, 0, SEEK_END) != 0) {
    fprintf(stderr, "Error: Failed to seek to end of file '%s': %s\n", filepath,
            strerror(errno));
    fclose(file);
    return NULL;
  }

  long file_size = ftell(file);
  if (file_size < 0) {
    fprintf(stderr, "Error: Failed to get file size for '%s': %s\n", filepath,
            strerror(errno));
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
  char *buffer = (char *)malloc((size_t)file_size + 1);
  if (!buffer) {
    fprintf(stderr,
            "Error: Memory allocation failed for file content (size: %ld).\n",
            file_size);
    fclose(file);
    return NULL;
  }

  size_t read_size = fread(buffer, 1, (size_t)file_size, file);
  if (read_size == 0) {
    fprintf(stderr, "Error: Failed to read from file '%s': %s\n", filepath,
            strerror(errno));
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
