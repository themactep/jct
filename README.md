# JSON Configuration CLI Tool

A command-line tool for reading and writing JSON configuration files.
This tool is written in pure C with no external dependencies, making
it suitable for cross-compilation and embedded systems.

## Features

- Read values from JSON configuration files using dot notation
- Write values to JSON configuration files using dot notation
- Create new JSON configuration files
- Print entire JSON configuration files
- Restore configuration files to original state (OverlayFS support)
- Support for nested objects and arrays
- Support for various data types (strings, numbers, booleans, null)
- Pretty-printing of JSON output

## Building

### Native Compilation

To build the project for your local machine, run:

```bash
make
```

This will compile the source files and create the `jct` executable.

### Cross-Compilation

#### Toolchain Selection

The Makefile supports multiple toolchains using the standard `CROSS_COMPILE` prefix:

```bash
# Use prefix for a toolchain found in PATH
make CROSS_COMPILE=mipsel-linux-gnu-

# Use any custom toolchain with a full path
make CROSS_COMPILE=/path/to/toolchain/bin/prefix-
```

You can also use these toolchain options with the release target:

```bash
make CROSS_COMPILE=/path/to/toolchain/bin/mipsel-linux-musl- release
make CROSS_COMPILE=mipsel-linux-gnu- release
```

### Optimized Builds

For optimized builds with smaller binary size:

```bash
make release         # Optimized build (stripped of debug info)
```

The optimized builds use the following techniques to reduce binary size:
- `-Os` optimization flag for size
- Removal of unused code with `-ffunction-sections` and `-fdata-sections`
- Stripping of debug information

### Cleaning

To clean up the build artifacts:

```bash
make clean           # Remove object files and executables
make distclean       # Remove all generated files (complete cleanup)
```

## Usage

```
Usage: jct [--trace-resolve] <config_file> <command> [options]

Commands:
  <config_file> get <key>              Get a value from the config file
  <config_file> set <key> <value>      Set a value in the config file
  <config_file> create                 Create a new empty config file
  <config_file> print                  Print the entire config file
  <config_file> restore                Restore config file to original state (OverlayFS)

Options:
  --trace-resolve                      Trace short-name resolution steps

Short-name resolution (when <config_file> has no '/' and does not end with .json):
  Tries, in order: ./<name>, ./<name>.json, /etc/<name>.json (POSIX only)
  - If none found: exit 2 with an error listing tried paths
  - If a candidate exists but is not readable: exit 13 (do not try later candidates)
  - Symlinks are followed; final target must be a regular file (directories are skipped)

Creation rules:
  - create requires an explicit path
  - set may create a new file only with an explicit path
  - Using a short name that does not resolve will not create; the command exits 2 with guidance

Examples:
  jct prudynt get server.host           Resolve short name 'prudynt' and read
  jct prudynt set app.name "My App"     Resolve and update existing; to create, use explicit path
  jct ./prudynt set app.name "My App"   Explicit path; allowed to create
  jct config.json print                 Print the entire config file
```
### Exit codes

- 0: Success
- 2: Not found
  - Short name did not resolve to any candidate (get/print/restore/set)
  - Short name used with create (and with set when it would create) — use an explicit path instead
- 13: Permission denied
  - A candidate file was found during short-name resolution but is not readable; later candidates are not tried

Note: The restore command defines additional exit codes specific to OverlayFS operations; see the Restore section below for details.

### Short-name resolution details

When <config_file> is a short name (no path separators and no .json extension), jct searches for a JSON file deterministically:

1. ./<name>
2. ./<name>.json
3. /etc/<name>.json (POSIX systems only)

Rules:
- If a candidate does not exist, jct continues to the next.
- If a candidate exists and is a directory, it is skipped.
- If a candidate exists, symlinks are followed and the final target must be a regular file.
- If a candidate is a regular file but not readable, jct exits immediately with code 13 (permission denied) and does not try later candidates.
- If no candidate is selected, jct exits with code 2 (not found) and prints:
  jct: no JSON file found for '<name>'; tried: ./<name>, ./<name>.json[, /etc/<name>.json]

Creation behavior:
- create requires an explicit path; short names are not accepted for creation and will return code 2 with guidance.
- set may create a new file only when invoked with an explicit path; using a short name that does not resolve will return code 2 and advise supplying an explicit path (e.g., ./<name>.json).

Tip:
- Use --trace-resolve to print the resolution steps and the final chosen path to stderr.


### Examples

#### Creating a new configuration file

```bash
./jct new_config.json create
```

#### Setting values in a configuration file

```bash
./jct config.json set server.host localhost
./jct config.json set server.port 8080
./jct config.json set server.ssl true
./jct config.json set app.name "My Application"
./jct config.json set app.version 1.0
```

#### JSONPath queries (new)

Query JSON data using Goessner JSONPath.

Examples:

- jct books.json path "$..author" --mode values
- jct books.json path "$.store.book[?(@.price < 10)].title"
- jct books.json path "$.store.book[0:3]" --mode pairs

Options:

- --mode values|paths|pairs (default: values)
- --limit N to stop after N matches
- --strict causes parse/eval errors to exit nonzero (2 parse, 3 eval); default lenient emits [] and warns to stderr
- --pretty pretty-prints JSON output
- --unwrap-single when mode=values, emit the lone value instead of [value]


#### Getting values from a configuration file

```bash
./jct config.json get server.host
# Output: localhost

./jct config.json get server.port
# Output: 8080

./jct config.json get app.name
# Output: My Application
```

#### Printing the entire configuration file

```bash
./jct config.json print
```

#### Restoring a configuration file to its original state (OverlayFS)

```bash
./jct /etc/config.json restore
```

This command is designed for embedded systems using OverlayFS. It:
1. **Requires an absolute path** (must start with '/') for the config file
2. Validates that the original file exists in `/rom/<config_file>`
3. Checks that a modified version exists in `/overlay/<config_file>`
4. Removes the overlay file to expose the original ROM version
5. Remounts the overlay filesystem to apply changes

**Important:** The config file path must be absolute. Relative paths are not accepted.

**Examples:**
```bash
./jct /etc/prudynt.json restore          # ✓ Valid - absolute path
./jct /opt/app/config.json restore       # ✓ Valid - absolute path
./jct prudynt.json restore               # ✗ Invalid - relative path
./jct ./config.json restore              # ✗ Invalid - relative path
```

**Exit codes:**
- 0: Success - file restored to original state
- 1: Original ROM file not found
- 2: File is already original (no overlay to remove)
- 3: Failed to remove overlay file
- 4: Failed to remount overlay filesystem
- 5: Invalid arguments or non-absolute path

## Project Structure

- `src/json_config.h` - Header file with type definitions and function declarations
- `src/json_value.c` - Implementation of JSON value handling functions
- `src/json_parse.c` - Implementation of JSON parsing functions
- `src/json_serialize.c` - Implementation of JSON serialization functions
- `src/json_config.c` - Implementation of configuration manipulation functions
- `src/json_config_cli.c` - Main file with CLI interface
- `Makefile` - Build configuration

## License

This project is open source and available under the MIT License.
