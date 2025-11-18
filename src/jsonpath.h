#ifndef JSONPATH_H
#define JSONPATH_H

#include "json_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Modes for output formatting
typedef enum {
  JSONPATH_MODE_VALUES = 0,
  JSONPATH_MODE_PATHS = 1,
  JSONPATH_MODE_PAIRS = 2
} JsonPathMode;

// Options controlling evaluation behavior
typedef struct {
  JsonPathMode mode; // values | paths | pairs
  int limit;         // <=0 means no limit
  int strict; // non-zero -> strict errors; zero -> lenient (empty results)
} JsonPathOptions;

// Results container
typedef struct {
  JsonPathMode mode;  // echo back mode
  int count;          // number of results
  char **paths;       // when mode==PATHS or PAIRS (string JSONPaths)
  JsonValue **values; // when mode==VALUES or PAIRS (deep-cloned JsonValues)
} JsonPathResults;

// Parse and evaluate a JSONPath expression against a JSON document.
// On success returns non-NULL results (possibly count==0). On failure:
//  - strict: returns NULL and writes a diagnostic to stderr
//  - lenient: returns an empty results object (count==0)
JsonPathResults *evaluate_jsonpath(JsonValue *doc, const char *expression,
                                   const JsonPathOptions *options);

// Frees a results object produced by evaluate_jsonpath
void free_jsonpath_results(JsonPathResults *res);

#ifdef __cplusplus
}
#endif

#endif // JSONPATH_H
