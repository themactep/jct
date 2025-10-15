# Changelog

## Unreleased

- Added first-class JSONPath support (Goessner baseline) to CLI and library:
  - New `jct jsonpath <expression>` command with options: `--mode values|paths|pairs`, `--limit N`, `--strict`, `--pretty`, `--stdin`, `-f file.json`
  - Programmatic API: `evaluate_jsonpath(document, expression, options)` in C (see `src/jsonpath.h`)
  - Supports: root `$`, dot and bracket child access, recursive descent `..`, wildcard `*`, array indexing, unions, slices, and basic filters with `@` and boolean operators (`&&`, `||`, `!`) plus comparisons (`==`, `!=`, `<`, `<=`, `>`, `>=`)
  - Outputs: values, paths, or pairs; stable document order; no deduplication
  - Strict vs lenient mode error policy
- Added tests and fixtures for JSONPath (`test/books.json`) and extended `test/run_tests.sh`
- Updated README and CLI usage

