/**
 * json_config_cli.c - Main file for the JSON configuration CLI tool
 */

#include "json_config.h"
#include "jsonpath.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// --- Short-name JSON file resolution helpers ---
static int has_path_separator(const char *s) {
  for (const char *p = s; *p; ++p) {
    if (*p == '/' || *p == '\\')
      return 1;
  }
  return 0;
}

static int ends_with_json_ext(const char *s) {
  size_t n = strlen(s);
  return (n >= 5 && strcmp(s + (n - 5), ".json") == 0);
}

static int is_regular_file_following_symlink(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0)
    return 0;
  return S_ISREG(st.st_mode);
}

static int is_directory_following_symlink(const char *path) {
  struct stat st;
  if (stat(path, &st) != 0)
    return 0;
  return S_ISDIR(st.st_mode);
}

static int path_exists(const char *path) {
  return access(path, F_OK) == 0;
}

static int path_readable(const char *path) {
  return access(path, R_OK) == 0;
}

// Returns 0 on success and writes resolved path into out_path.
// On failure returns 2 (not found) or 13 (permission denied).
// When trace is non-zero, prints candidate evaluation to stderr.
static int resolve_config_target(const char *target, int trace, char *out_path,
                                 size_t out_sz) {
  // Rule 1: explicit path (contains path separator or ends with .json) -> use
  // as-is
  if (has_path_separator(target) || ends_with_json_ext(target)) {
    if (trace) {
      fprintf(stderr, "[trace] explicit path used: %s\n", target);
    }
    // Use as-is; do not validate here
    if (snprintf(out_path, out_sz, "%s", target) >= (int)out_sz) {
      // Truncated, treat as not found
      return 2;
    }
    return 0;
  }

  // Otherwise short name search
  char cand1[PATH_MAX];
  char cand2[PATH_MAX];
  char cand3[PATH_MAX];
  int have_cand3 = 0;

  snprintf(cand1, sizeof(cand1), "./%s", target);
  snprintf(cand2, sizeof(cand2), "./%s.json", target);
#ifndef _WIN32
  snprintf(cand3, sizeof(cand3), "/etc/%s.json", target);
  have_cand3 = 1;
#endif

  const char *candidates[3];
  int cand_count = 0;
  candidates[cand_count++] = cand1;
  candidates[cand_count++] = cand2;
  if (have_cand3)
    candidates[cand_count++] = cand3;

  for (int i = 0; i < cand_count; ++i) {
    const char *c = candidates[i];
    if (trace)
      fprintf(stderr, "[trace] checking %s... ", c);
    if (!path_exists(c)) {
      if (trace)
        fprintf(stderr, "not found\n");
      continue;
    }
    if (is_directory_following_symlink(c)) {
      if (trace)
        fprintf(stderr, "is a directory, skip\n");
      continue;
    }
    if (!is_regular_file_following_symlink(c)) {
      if (trace)
        fprintf(stderr, "not a regular file, skip\n");
      continue;
    }
    // Exists and is a regular file -> check readability
    if (!path_readable(c)) {
      if (trace)
        fprintf(stderr, "exists but not readable -> permission denied\n");
      // Do not fall back to later candidates
      fprintf(stderr, "jct: permission denied: %s\n", c);
      return 13;
    }
    // Select this candidate
    if (snprintf(out_path, out_sz, "%s", c) >= (int)out_sz) {
      // Truncation shouldn't happen with PATH_MAX, but handle anyway
      return 2;
    }
    if (trace)
      fprintf(stderr, "selected\n[trace] resolved to: %s\n", out_path);
    return 0;
  }

  // None found
  if (trace)
    fprintf(stderr, "[trace] no matching file found for '%s'\n", target);
  // Build error message listing tried candidates
  fprintf(stderr, "jct: no JSON file found for '%s'; tried: %s, %s", target,
          cand1, cand2);
  if (have_cand3)
    fprintf(stderr, ", %s", cand3);
  fprintf(stderr, "\n");
  return 2;
}

// --- JSONPath (path) command handler ---
static int handle_path_command(const char *config_file, int argc, char *argv[],
                               int start_index) {
  // Syntax: jct <file> path <expression> [--mode values|paths|pairs] [--limit
  // N] [--strict] [--pretty] [--unwrap-single]
  const char *expr = NULL;
  int pretty = 0;
  int unwrap_single = 0;
  JsonPathOptions opt = {.mode = JSONPATH_MODE_VALUES, .limit = 0, .strict = 0};
  for (int i = start_index; i < argc; ++i) {
    const char *a = argv[i];
    if (!expr && a[0] != '-') {
      expr = a;
      continue;
    }
    if (strcmp(a, "--mode") == 0 && i + 1 < argc) {
      const char *m = argv[++i];
      if (strcmp(m, "values") == 0)
        opt.mode = JSONPATH_MODE_VALUES;
      else if (strcmp(m, "paths") == 0)
        opt.mode = JSONPATH_MODE_PATHS;
      else if (strcmp(m, "pairs") == 0)
        opt.mode = JSONPATH_MODE_PAIRS;
      else {
        fprintf(stderr, "Error: invalid --mode '%s'\n", m);
        return 2;
      }
    } else if (strcmp(a, "--limit") == 0 && i + 1 < argc) {
      opt.limit = atoi(argv[++i]);
      if (opt.limit < 0)
        opt.limit = 0;
    } else if (strcmp(a, "--strict") == 0) {
      opt.strict = 1;
    } else if (strcmp(a, "--pretty") == 0) {
      pretty = 1;
    } else if (strcmp(a, "--unwrap-single") == 0) {
      unwrap_single = 1;
    } else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
      printf("Usage: jct <file.json> path <expression> [--mode "
             "values|paths|pairs] [--limit N] [--strict] [--pretty] "
             "[--unwrap-single]\n");
      return 0;
    } else if (!expr) {
      expr = a;
    } else {
      fprintf(stderr, "Error: unknown argument '%s'\n", a);
      return 2;
    }
  }
  if (!expr) {
    fprintf(stderr, "Error: path requires an expression.\n");
    return 2;
  }

  JsonValue *doc = parse_json_file(config_file);
  if (!doc) {
    return opt.strict ? 3 : 0;
  }

  JsonPathResults *res = evaluate_jsonpath(doc, expr, &opt);
  if (!res) {
    free_json_value(doc);
    return opt.strict ? 2 : 0;
  }

  // Unwrap single for values mode if requested
  if (opt.mode == JSONPATH_MODE_VALUES && unwrap_single && res->count == 1) {
    char *scalar = json_to_string(res->values[0], pretty);
    if (!scalar)
      scalar = strdup("null");
    printf("%s\n", scalar);
    free(scalar);
    free_jsonpath_results(res);
    free_json_value(doc);
    return 0;
  }

  JsonValue *out_json = create_json_value(JSON_ARRAY);
  if (!out_json) {
    free_jsonpath_results(res);
    free_json_value(doc);
    return opt.strict ? 3 : 0;
  }

  if (res->mode == JSONPATH_MODE_VALUES) {
    for (int i = 0; i < res->count; ++i) {
      add_to_array(out_json, res->values[i]);
      res->values[i] = NULL;
    }
  } else if (res->mode == JSONPATH_MODE_PATHS) {
    for (int i = 0; i < res->count; ++i) {
      JsonValue *s = create_json_value(JSON_STRING);
      s->value.string = strdup(res->paths[i] ? res->paths[i] : "$");
      add_to_array(out_json, s);
    }
  } else { // pairs
    for (int i = 0; i < res->count; ++i) {
      JsonValue *obj = create_json_value(JSON_OBJECT);
      // Put 'value' then 'path' so printing order matches expected
      add_to_object(obj, "value", res->values[i]);
      res->values[i] = NULL;
      JsonValue *sp = create_json_value(JSON_STRING);
      sp->value.string = strdup(res->paths[i] ? res->paths[i] : "$");
      add_to_object(obj, "path", sp);
      add_to_array(out_json, obj);
    }
  }

  char *out_str = json_to_string(out_json, pretty);
  if (!out_str)
    out_str = strdup("[]");
  printf("%s\n", out_str);
  free(out_str);
  free_json_value(out_json);
  free_jsonpath_results(res);
  free_json_value(doc);
  return 0;
}

// Function to print usage information
static void print_usage(void) {
  printf("Usage: jct [--trace-resolve] <config_file> <command> [options]\n\n");
  printf("Commands:\n");
  printf("  <config_file> get <key>              Get a value from the config "
         "file\n");
  printf("  <config_file> set <key> <value>      Set a value in the config "
         "file\n");
  printf("  <config_file> import <source_file>    Merge values from another "
         "JSON file\n");
  printf("  <config_file> create                 Create a new empty config "
         "file\n");
  printf("  <config_file> print                  Print the entire config "
         "file\n");
  printf("  <config_file> restore                Restore config file to "
         "original state (OverlayFS)\n");
  printf("  <config_file> path <expression>      Query JSON using JSONPath "
         "(Goessner)\n");
  printf("\n");
  printf("Options:\n");
  printf("  --trace-resolve                      Trace short-name resolution "
         "steps (get/set/import/print/restore)\n");
  printf("  path options: --mode values|paths|pairs [--limit N] [--strict] "
         "[--pretty] [--unwrap-single]\n");
  printf("\n");
  printf("Short-name resolution (when <config_file> has no '/' and no "
         "'.json'):\n");
  printf("  Tries: ./<name>, ./<name>.json, /etc/<name>.json (POSIX).\n");
  printf("  If none found: exit 2 with list of tried paths. If unreadable: "
         "exit 13.\n");
  printf("Creation rules:\n");
  printf("  'create' requires an explicit path. 'set' may create only with an "
         "explicit path.\n");
  printf("\n");
  printf("Examples:\n");
  printf("  jct prudynt get server.port           Resolve short name 'prudynt' "
         "and read\n");
  printf("  jct prudynt set app.name 'My App'     Resolve and update existing; "
         "to create, use explicit path\n");
  printf("  jct ./prudynt set app.name 'My App'   Explicit path; allowed to "
         "create\n");
  printf("  jct config.json print                 Print the entire config "
         "file\n");
  printf("  jct /etc/config.json restore          Restore /etc/config.json "
         "(absolute path required)\n");
  printf("  jct books.json path '$..author' --mode values\n");
}

// Function to handle the 'get' command
static int handle_get_command(const char *config_file, const char *key) {
  JsonValue *config = load_config(config_file);
  if (!config) {
    fprintf(stderr, "Error: Failed to load config file '%s'.\n", config_file);
    return 1;
  }

  JsonValue *value = get_nested_item(config, key);
  if (!value) {
    fprintf(stderr, "Error: Key '%s' not found in config file.\n", key);
    free_json_value(config);
    return 1;
  }

  print_item(value);
  free_json_value(config);
  return 0;
}

// Function to handle the 'set' command
static int handle_set_command(const char *config_file, const char *key,
                              const char *value_str) {
  JsonValue *config = load_config(config_file);
  if (!config) {
    // If the file doesn't exist, create a new empty config
    config = create_json_value(JSON_OBJECT);
    if (!config) {
      fprintf(stderr, "Error: Failed to create new config object.\n");
      return 1;
    }
  }

  if (set_nested_item(config, key, value_str)) {
    if (save_config(config_file, config)) {
      // Silent success - no output
      free_json_value(config);
      return 0;
    } else {
      fprintf(stderr, "Error: Failed to save config file '%s'.\n", config_file);
      free_json_value(config);
      return 1;
    }
  } else {
    fprintf(stderr, "Error: Failed to set key '%s' in config file.\n", key);
    free_json_value(config);
    return 1;
  }
}

// Function to handle the 'create' command
static int handle_create_command(const char *config_file) {
  // Check if the file already exists
  if (access(config_file, F_OK) == 0) {
    fprintf(stderr, "Error: Config file '%s' already exists.\n", config_file);
    return 1;
  }

  // Create a new empty config object
  JsonValue *config = create_json_value(JSON_OBJECT);
  if (!config) {
    fprintf(stderr, "Error: Failed to create new config object.\n");
    return 1;
  }

  // Save the config to the file
  if (save_config(config_file, config)) {
    // Silent success - no output for create command
    free_json_value(config);
    return 0;
  } else {
    fprintf(stderr, "Error: Failed to save config file '%s'.\n", config_file);
    free_json_value(config);
    return 1;
  }
}

// Function to handle the 'print' command
static int handle_print_command(const char *config_file) {
  JsonValue *config = load_config(config_file);
  if (!config) {
    fprintf(stderr, "Error: Failed to load config file '%s'.\n", config_file);
    return 1;
  }

  print_item(config);
  free_json_value(config);
  return 0;
}

// Function to handle the 'restore' command
static int handle_restore_command(const char *config_file) {
  char rom_path[PATH_MAX];
  char overlay_path[PATH_MAX];

  // Validate input
  if (!config_file || strlen(config_file) == 0) {
    fprintf(stderr, "Error: Invalid config file path.\n");
    return 5;
  }

  // Require absolute path
  if (config_file[0] != '/') {
    fprintf(stderr,
            "Error: Config file path must be absolute (start with '/'). Got: "
            "'%s'\n",
            config_file);
    return 5;
  }

  // Build ROM and overlay paths using the absolute path
  if (snprintf(rom_path, sizeof(rom_path), "/rom%s", config_file) >=
      (int)sizeof(rom_path)) {
    fprintf(stderr, "Error: ROM path too long.\n");
    return 5;
  }
  if (snprintf(overlay_path, sizeof(overlay_path), "/overlay%s", config_file) >=
      (int)sizeof(overlay_path)) {
    fprintf(stderr, "Error: Overlay path too long.\n");
    return 5;
  }

  // Check if ROM file exists
  if (access(rom_path, F_OK) != 0) {
    fprintf(stderr, "Error: Original file '%s' not found\n", rom_path);
    return 1;
  }

  // Check if overlay file exists
  if (access(overlay_path, F_OK) != 0) {
    fprintf(stderr, "Error: The file is original, nothing to restore\n");
    return 2;
  }

  // Remove the overlay file
  if (unlink(overlay_path) != 0) {
    fprintf(stderr, "Error: Failed to remove overlay file '%s': %s\n",
            overlay_path, strerror(errno));
    return 3;
  }

  // Remount the overlay filesystem
  if (system("mount -o remount /") != 0) {
    fprintf(stderr, "Error: Failed to remount overlay filesystem: %s\n",
            strerror(errno));
    return 4;
  }

  // Silent success - no output for restore command
  return 0;
}

// Function to handle the 'import' command
static int handle_import_command(const char *dest_file,
                                 const char *source_file) {
  JsonValue *dest = load_config(dest_file);
  if (!dest) {
    dest = create_json_value(JSON_OBJECT);
    if (!dest) {
      fprintf(stderr,
              "Error: Failed to create destination object for '%s'.\n",
              dest_file);
      return 1;
    }
  }

  JsonValue *source = load_config(source_file);
  if (!source) {
    fprintf(stderr, "Error: Failed to load source file '%s'.\n", source_file);
    free_json_value(dest);
    return 1;
  }

  if (!merge_json_into(&dest, source)) {
    fprintf(stderr, "Error: Failed to merge '%s' into '%s'.\n", source_file,
            dest_file);
    free_json_value(source);
    free_json_value(dest);
    return 1;
  }

  if (!save_config(dest_file, dest)) {
    fprintf(stderr, "Error: Failed to save merged config to '%s'.\n",
            dest_file);
    free_json_value(source);
    free_json_value(dest);
    return 1;
  }

  free_json_value(source);
  free_json_value(dest);
  return 0;
}

int main(int argc, char *argv[]) {
  // Gather non-flag arguments and recognize --trace-resolve
  int trace_resolve = 0;
  int idxs[argc];
  int nidx = 0;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--trace-resolve") == 0) {
      trace_resolve = 1;
      continue;
    }
    idxs[nidx++] = i;
  }

  if (nidx < 2) {
    print_usage();
    return 1;
  }

  const char *config_target = argv[idxs[0]];
  const char *command = argv[idxs[1]];

  char resolved_path[PATH_MAX];
  const char *cfg_for_handlers = config_target;

  // Decide path handling per command
  if (strcmp(command, "get") == 0 || strcmp(command, "print") == 0 ||
      strcmp(command, "restore") == 0 || strcmp(command, "path") == 0) {
    // These require an existing readable file; apply short-name resolution
    int rc = resolve_config_target(config_target, trace_resolve, resolved_path,
                                   sizeof(resolved_path));
    if (rc != 0) {
      return rc; // 2 not found (already printed), or 13 permission denied
    }
    cfg_for_handlers = resolved_path;
  } else if (strcmp(command, "set") == 0) {
    // set: short-name must resolve to existing file; explicit path may
    // create
    if (!(has_path_separator(config_target) ||
          ends_with_json_ext(config_target))) {
      // short name
      int rc = resolve_config_target(config_target, trace_resolve,
                                     resolved_path, sizeof(resolved_path));
      if (rc != 0) {
        if (rc == 2) {
          // Add guidance for creating via explicit path
          fprintf(stderr,
                  "jct: to create a new file, supply an explicit "
                  "path (e.g., "
                  "./%s.json)\n",
                  config_target);
        }
        return rc; // 2/13 exit
      }
      cfg_for_handlers = resolved_path; // resolved existing file
    }
    // explicit path: pass as-is (allows creation)
    cfg_for_handlers = config_target;
  } else if (strcmp(command, "create") == 0) {
    // create always requires an explicit path
    if (!(has_path_separator(config_target) ||
          ends_with_json_ext(config_target))) {
      fprintf(stderr,
              "jct: 'create' requires an explicit path; to create a new "
              "file, "
              "supply an explicit path (e.g., ./%s.json)\n",
              config_target);
      return 2;
    }
    cfg_for_handlers = config_target; // explicit path
  } else if (strcmp(command, "import") == 0) {
    if (nidx < 3) {
      fprintf(stderr, "Error: 'import' command requires a source file.\n");
      print_usage();
      return 1;
    }

    const char *source_target = argv[idxs[2]];
    const char *dest_path = config_target;
    char dest_resolved[PATH_MAX];

    if (!(has_path_separator(config_target) ||
          ends_with_json_ext(config_target))) {
      int rc = resolve_config_target(config_target, trace_resolve,
                                     dest_resolved, sizeof(dest_resolved));
      if (rc != 0) {
        if (rc == 2) {
          fprintf(stderr,
                  "jct: to create a new file, supply an explicit path (e.g., "
                  "./%s.json)\n",
                  config_target);
        }
        return rc;
      }
      dest_path = dest_resolved;
    }

    char source_resolved[PATH_MAX];
    int rc = resolve_config_target(source_target, trace_resolve,
                                   source_resolved,
                                   sizeof(source_resolved));
    if (rc != 0) {
      return rc;
    }
    return handle_import_command(dest_path, source_resolved);
  }

  // Dispatch
  if (strcmp(command, "get") == 0) {
    if (nidx < 3) {
      fprintf(stderr, "Error: 'get' command requires a key.\n");
      print_usage();
      return 1;
    }
    return handle_get_command(cfg_for_handlers, argv[idxs[2]]);
  } else if (strcmp(command, "set") == 0) {
    if (nidx < 4) {
      fprintf(stderr, "Error: 'set' command requires a key and a value.\n");
      print_usage();
      return 1;
    }
    return handle_set_command(cfg_for_handlers, argv[idxs[2]], argv[idxs[3]]);
  } else if (strcmp(command, "create") == 0) {
    return handle_create_command(cfg_for_handlers);
  } else if (strcmp(command, "print") == 0) {
    return handle_print_command(cfg_for_handlers);
  } else if (strcmp(command, "restore") == 0) {
    return handle_restore_command(cfg_for_handlers);
  } else if (strcmp(command, "path") == 0) {
    if (nidx < 3) {
      fprintf(stderr, "Error: 'path' command requires an expression.\n");
      print_usage();
      return 1;
    }
    return handle_path_command(cfg_for_handlers, argc, argv, idxs[2]);
  } else if (strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
    print_usage();
    return 0;
  } else {
    fprintf(stderr, "Error: Unknown command '%s'.\n", command);
    print_usage();
    return 1;
  }
}
