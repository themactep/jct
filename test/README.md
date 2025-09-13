# JCT Test Suite

This directory contains a comprehensive test suite for the JSON Configuration Tool (jct).

## Files

- `test_data.json` - Comprehensive test data file with various JSON data types and edge cases
- `run_tests.sh` - Main test script that runs all tests
- `README.md` - This file

## Running Tests

To run the complete test suite:

```bash
make test
```

Or run the test script directly:

```bash
./test/run_tests.sh
```

## Test Coverage

The test suite covers:

### Core Functionality
- **String values**: Simple strings, empty strings, strings with spaces, numeric-looking strings, version strings, boolean-looking strings, null-looking strings
- **Number values**: Integers, negative numbers, zero, floats, negative floats
- **Boolean values**: true and false values
- **Null values**: explicit null values

### Advanced Features
- **Nested access**: Deep nested object access using dot notation
- **Array access**: Accessing array elements by index
- **Edge cases**: Whitespace-only strings, path-like strings, URL-like strings, email-like strings

### Error Handling
- Non-existent keys
- Non-existent files
- Invalid array indices

### Commands
- **get**: Retrieving values from JSON files
- **set**: Setting values in JSON files
- **create**: Creating new JSON configuration files
- **print**: Printing entire JSON structures

### Operations
- **Create and set**: Creating new configs and setting various data types
- **Nested operations**: Setting and getting deeply nested values
- **Array operations**: Setting and getting array elements
- **Overwrite operations**: Updating existing values
- **Special value parsing**: Ensuring proper type detection (strings vs numbers vs booleans)

## Test Data Structure

The `test_data.json` file contains:

- `strings`: Various string types and edge cases
- `numbers`: Different number formats (integers, floats, scientific notation)
- `booleans`: Boolean values
- `null_values`: Null values
- `arrays`: Empty arrays, string arrays, number arrays, mixed arrays, nested arrays
- `objects`: Empty objects, simple objects, nested objects, mixed-type objects
- `edge_cases`: Special characters, whitespace, newlines, long strings, path-like strings
- `config_examples`: Real-world configuration examples (server, database, features, logging)

## Expected Behavior

The tests verify that jct correctly:

1. **Outputs scalar values without JSON formatting**:
   - Strings are output without surrounding quotes
   - Numbers are output as plain numbers
   - Booleans are output as "true" or "false"
   - Null is output as "null"

2. **Maintains JSON formatting for complex structures**:
   - Objects and arrays are output with proper JSON formatting
   - Nested structures maintain proper indentation

3. **Handles type detection correctly**:
   - Version strings like "1.2.3" remain as strings
   - Pure numbers are stored as numbers
   - Empty strings are handled correctly (not converted to numbers)

4. **Supports all CRUD operations**:
   - Create new configuration files
   - Read values using dot notation
   - Update existing values
   - Handle nested object and array operations

## Notes

- The special characters test is currently skipped due to JSON escape sequence handling complexity in the current parser implementation
- All tests use the same test data file to ensure consistency
- Temporary files created during testing are automatically cleaned up
- The test suite provides colored output for easy identification of passed/failed tests
