/**
 * json_config_cli.c - Main file for the JSON configuration CLI tool
 */

#include "json_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

// Function to print usage information
static void print_usage(void) {
    printf("Usage: jct <config_file> <command> [options]\n\n");
    printf("Commands:\n");
    printf("  <config_file> get <key>              Get a value from the config file\n");
    printf("  <config_file> set <key> <value>      Set a value in the config file\n");
    printf("  <config_file> create                 Create a new empty config file\n");
    printf("  <config_file> print                  Print the entire config file\n");
    printf("  <config_file> restore                Restore config file to original state (OverlayFS)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  jct config.json get server.host       Get the server host from config.json\n");
    printf("  jct config.json set server.port 8080  Set the server port to 8080 in config.json\n");
    printf("  jct new_config.json create            Create a new empty config file\n");
    printf("  jct config.json print                 Print the entire config file\n");
    printf("  jct /etc/config.json restore          Restore /etc/config.json to original state (absolute path required)\n");
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
static int handle_set_command(const char *config_file, const char *key, const char *value_str) {
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
        fprintf(stderr, "Error: Config file path must be absolute (start with '/'). Got: '%s'\n", config_file);
        return 5;
    }

    // Build ROM and overlay paths using the absolute path
    if (snprintf(rom_path, sizeof(rom_path), "/rom%s", config_file) >= (int)sizeof(rom_path)) {
        fprintf(stderr, "Error: ROM path too long.\n");
        return 5;
    }
    if (snprintf(overlay_path, sizeof(overlay_path), "/overlay%s", config_file) >= (int)sizeof(overlay_path)) {
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

int main(int argc, char *argv[]) {
    // Check if enough arguments are provided
    if (argc < 3) {
        print_usage();
        return 1;
    }

    const char *config_file = argv[1];
    const char *command = argv[2];

    // Handle the different commands
    if (strcmp(command, "get") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: 'get' command requires a key.\n");
            print_usage();
            return 1;
        }
        return handle_get_command(config_file, argv[3]);
    } else if (strcmp(command, "set") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Error: 'set' command requires a key and a value.\n");
            print_usage();
            return 1;
        }
        return handle_set_command(config_file, argv[3], argv[4]);
    } else if (strcmp(command, "create") == 0) {
        return handle_create_command(config_file);
    } else if (strcmp(command, "print") == 0) {
        return handle_print_command(config_file);
    } else if (strcmp(command, "restore") == 0) {
        return handle_restore_command(config_file);
    } else if (strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        print_usage();
        return 0;
    } else {
        fprintf(stderr, "Error: Unknown command '%s'.\n", command);
        print_usage();
        return 1;
    }
}
