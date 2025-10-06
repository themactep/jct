# Bug Fix: JSON Escape Sequence Handling

## Problem Description

The `jct` tool had a critical bug where backslashes in string values were being doubled/escaped every time the file was saved with any modification.

### Reproduction Steps

1. Create a new JSON file: `touch a.json`
2. Set initial value with escaped quotes: `jct a.json set a "\"a\""`
3. Verify the file contains: `{"a": "\"a\""}`
4. Make any subsequent change: `jct a.json set b 1`
5. Observe that the original value now has doubled backslashes: `{"a": "\\\"a\\\""}`

Each subsequent save would continue to double the backslashes:
- After 1st save: `"\"a\""`
- After 2nd save: `"\\\"a\\\""`
- After 3rd save: `"\\\\\"a\\\\\""`
- And so on...

## Root Cause

The bug was in the `parse_string()` function in `src/json_parse.c`. The function had a comment on line 78 that said "Simple copy without handling escapes properly".

### The Problem Flow

1. **Serialization (Writing)**: When writing JSON, the `escape_string()` function in `json_serialize.c` correctly escapes special characters:
   - `"` becomes `\"`
   - `\` becomes `\\`
   - `\n` becomes `\\n`
   - etc.

2. **Parsing (Reading)**: When reading JSON back, the `parse_string()` function was **NOT** unescaping these sequences. It was copying them literally:
   - The JSON string `"\"a\""` was being stored in memory as the 4-character string: `\"a\"`
   - Instead of being stored as the 3-character string: `"a"`

3. **Re-serialization**: When saving again, `escape_string()` would escape the already-escaped string:
   - The in-memory string `\"a\"` would be escaped to `\\\"a\\\"`
   - This would continue to compound on each save

## Solution

The fix was to properly implement escape sequence handling in the `parse_string()` function:

### Changes Made to `src/json_parse.c`

The `parse_string()` function was rewritten to:

1. **First pass**: Count the actual length of the unescaped string
   - Iterate through the JSON string
   - Count each escape sequence as a single character
   - Find the closing quote

2. **Second pass**: Copy and unescape the string
   - Allocate memory for the unescaped string
   - Process each character:
     - If it's a backslash, set the `escaped` flag
     - If the `escaped` flag is set, convert the escape sequence:
       - `\"` → `"`
       - `\\` → `\`
       - `\n` → newline character
       - `\t` → tab character
       - `\r` → carriage return
       - `\b` → backspace
       - `\f` → form feed
       - `\/` → `/`
     - Otherwise, copy the character as-is

### Supported Escape Sequences

The fix properly handles all standard JSON escape sequences:
- `\"` - Double quote
- `\\` - Backslash
- `\n` - Newline
- `\t` - Tab
- `\r` - Carriage return
- `\b` - Backspace
- `\f` - Form feed
- `\/` - Forward slash

## Testing

A comprehensive test suite was created in `test/test_escape_sequences.sh` that verifies:

1. **Escaped quotes stability**: Values like `"a"` remain stable across multiple saves
2. **Backslashes in paths**: Windows-style paths like `C:\Users\test` remain stable
3. **Newline characters**: Multi-line strings remain stable
4. **Tab characters**: Tab-separated values remain stable
5. **Mixed escape sequences**: Strings with multiple types of escapes remain stable
6. **JSON format verification**: The JSON file format is correct after multiple saves

All 16 tests pass successfully.

## Verification

### Before the fix:
```bash
$ jct a.json set a '"a"'
$ cat a.json
{"a": "\"a\""}

$ jct a.json set b 1
$ cat a.json
{"a": "\\\"a\\\"", "b": 1}  # ❌ Backslashes doubled!
```

### After the fix:
```bash
$ jct a.json set a '"a"'
$ cat a.json
{"a": "\"a\""}

$ jct a.json set b 1
$ cat a.json
{"a": "\"a\"", "b": 1}  # ✅ Backslashes remain stable!
```

## Impact

This fix ensures that:
- String values with escape sequences are preserved correctly
- Multiple save operations don't corrupt the data
- The tool properly implements JSON parsing standards
- Users can safely use special characters in their configuration values

## Files Modified

- `src/json_parse.c`: Fixed the `parse_string()` function to properly unescape JSON escape sequences

## Files Added

- `test/test_escape_sequences.sh`: Comprehensive test suite for escape sequence handling
- `BUGFIX_ESCAPE_SEQUENCES.md`: This documentation file

