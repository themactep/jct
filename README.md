# JSON Configuration CLI Tool

A command-line tool for reading and writing JSON configuration files.
This tool is written in pure C with no external dependencies, making
it suitable for cross-compilation and embedded systems.

## Features

- Read values from JSON configuration files using dot notation
- Write values to JSON configuration files using dot notation
- Create new JSON configuration files
- Print entire JSON configuration files
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
Usage: jct <config_file> <command> [options]

Commands:
  <config_file> get <key>              Get a value from the config file
  <config_file> set <key> <value>      Set a value in the config file
  <config_file> create                 Create a new empty config file
  <config_file> print                  Print the entire config file

Examples:
  jct config.json get server.host       Get the server host from config.json
  jct config.json set server.port 8080  Set the server port to 8080 in config.json
  jct new_config.json create            Create a new empty config file
  jct config.json print                 Print the entire config file
```

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
