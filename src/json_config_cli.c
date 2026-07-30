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
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

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

// Function to handle the 'del' command
static int handle_del_command(const char *config_file, const char *key) {
  JsonValue *config = load_config(config_file);
  if (!config) {
    fprintf(stderr, "Error: Failed to load config file '%s'.\n", config_file);
    return 1;
  }

  if (!del_nested_item(config, key)) {
    /* Key not found is not an error — silently succeed */
  }

  if (save_config(config_file, config)) {
    free_json_value(config);
    return 0;
  } else {
    fprintf(stderr, "Error: Failed to save config file '%s'.\n", config_file);
    free_json_value(config);
    return 1;
  }
}

// Function to print usage information
static void print_usage(void) {
  printf("Usage: jct <config_file> <command> [options]\n\n");
  printf("Commands:\n");
  printf("  <config_file> get <key>              Get a value from the config "
         "file\n");
  printf("  <config_file> set <key> <value>      Set a value in the config "
         "file\n");
  printf("  <config_file> del <key>              Delete a key from the config "
         "file\n");
  printf("  <config_file> import <source_file>    Merge values from another "
         "JSON file\n");
  printf("  <config_file> export [<original_file>]\n");
  printf("                                       Export differences to stdout\n");
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
  printf("  path options: --mode values|paths|pairs [--limit N] [--strict] "
         "[--pretty] [--unwrap-single]\n");
  printf("\n");
  printf("Examples:\n");
  printf("  jct ./prudynt.json get server.port    Read a value\n");
  printf("  jct ./config.json set app.name 'My App'  Set a value\n");
  printf("  jct ./config.json print               Print the entire config "
         "file\n");
  printf("  jct /etc/prudynt.json export > diff.json\n");
  printf("                                        Export differences (compares "
         "with /rom version)\n");
  printf("  jct modified.json export base.json > diff.json\n");
  printf("                                        Export differences between "
         "two files\n");
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

// Function to handle the 'export' command
static int handle_export_command(const char *modified_file,
                                 const char *original_file) {
  // Determine the original file path
  char default_original[PATH_MAX];
  const char *original_path = original_file;

  if (!original_file) {
    // Default to /rom/<modified_file> for OverlayFS systems
    if (modified_file[0] == '/') {
      snprintf(default_original, sizeof(default_original), "/rom%s",
               modified_file);
      original_path = default_original;
    } else {
      fprintf(stderr,
              "Error: 'export' requires an explicit path or an original file "
              "to compare against.\n");
      return 1;
    }
  }

  JsonValue *modified = load_config(modified_file);
  if (!modified) {
    fprintf(stderr, "Error: Failed to load modified file '%s'.\n",
            modified_file);
    return 1;
  }

  JsonValue *original = load_config(original_path);
  if (!original) {
    fprintf(stderr, "Error: Failed to load original file '%s'.\n",
            original_path);
    if (!original_file) {
      fprintf(stderr,
              "Hint: Provide an explicit original file to compare against.\n");
    }
    free_json_value(modified);
    return 1;
  }

  JsonValue *diff = diff_json(modified, original);
  if (!diff) {
    fprintf(stderr, "Error: Failed to compute differences.\n");
    free_json_value(modified);
    free_json_value(original);
    return 1;
  }

  // Print diff to stdout
  print_item(diff);

  free_json_value(modified);
  free_json_value(original);
  free_json_value(diff);
  return 0;
}

int main(int argc, char *argv[]) {
  // Gather non-flag arguments
  int idxs[argc];
  int nidx = 0;
  for (int i = 1; i < argc; ++i) {
    idxs[nidx++] = i;
  }

  if (nidx < 2) {
    print_usage();
    return 1;
  }

  const char *config_target = argv[idxs[0]];
  const char *command = argv[idxs[1]];

  // Handle import and export early (they consume args differently)
  if (strcmp(command, "import") == 0) {
    if (nidx < 3) {
      fprintf(stderr, "Error: 'import' command requires a source file.\n");
      print_usage();
      return 1;
    }
    return handle_import_command(config_target, argv[idxs[2]]);
  } else if (strcmp(command, "export") == 0) {
    const char *original_file = (nidx >= 3) ? argv[idxs[2]] : NULL;
    return handle_export_command(config_target, original_file);
  }

  // Dispatch all other commands
  if (strcmp(command, "get") == 0) {
    if (nidx < 3) {
      fprintf(stderr, "Error: 'get' command requires a key.\n");
      print_usage();
      return 1;
    }
    return handle_get_command(config_target, argv[idxs[2]]);
  } else if (strcmp(command, "set") == 0) {
    if (nidx < 4) {
      fprintf(stderr, "Error: 'set' command requires a key and a value.\n");
      print_usage();
      return 1;
    }
    return handle_set_command(config_target, argv[idxs[2]], argv[idxs[3]]);
  } else if (strcmp(command, "del") == 0) {
    if (nidx < 3) {
      fprintf(stderr, "Error: 'del' command requires a key.\n");
      print_usage();
      return 1;
    }
    return handle_del_command(config_target, argv[idxs[2]]);
  } else if (strcmp(command, "create") == 0) {
    return handle_create_command(config_target);
  } else if (strcmp(command, "print") == 0) {
    return handle_print_command(config_target);
  } else if (strcmp(command, "restore") == 0) {
    return handle_restore_command(config_target);
  } else if (strcmp(command, "path") == 0) {
    if (nidx < 3) {
      fprintf(stderr, "Error: 'path' command requires an expression.\n");
      print_usage();
      return 1;
    }
    return handle_path_command(config_target, argc, argv, idxs[2]);
  } else if (strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
    print_usage();
    return 0;
  } else {
    fprintf(stderr, "Error: Unknown command '%s'.\n", command);
    print_usage();
    return 1;
  }
}
