# Changelog

## Unreleased

- **Breaking:** Removed short-name resolution. `<config_file>` must always be an explicit path.
  Removed `--trace-resolve` flag.
- Config writes are now atomic and durable: the temporary file is created next to the
  target instead of in `/tmp`, `fsync()`ed, then `rename()`d over the target. The
  cross-device copy fallback is gone; it truncated the live file in place before
  rewriting it, so concurrent readers could observe an empty or partial config and a
  crash mid-save could leave the file truncated. Applies to `save_config()` and
  `json_object_to_file_ext()`. An existing target's permissions are preserved across
  the replacement.
- Added first-class JSONPath support (Goessner baseline) to CLI and library:
  - New `jct jsonpath <expression>` command with options: `--mode values|paths|pairs`, `--limit N`, `--strict`, `--pretty`, `--stdin`, `-f file.json`
  - Programmatic API: `evaluate_jsonpath(document, expression, options)` in C (see `src/jsonpath.h`)
  - Supports: root `$`, dot and bracket child access, recursive descent `..`, wildcard `*`, array indexing, unions, slices, and basic filters with `@` and boolean operators (`&&`, `||`, `!`) plus comparisons (`==`, `!=`, `<`, `<=`, `>`, `>=`)
  - Outputs: values, paths, or pairs; stable document order; no deduplication
  - Strict vs lenient mode error policy
- Added tests and fixtures for JSONPath (`test/books.json`) and extended `test/run_tests.sh`
- Updated README and CLI usage

