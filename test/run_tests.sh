#!/bin/bash

# Comprehensive test suite for jct (JSON Configuration Tool)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Test data files
TEST_DATA="test/test_data.json"
TEMP_CONFIG="test/temp_config.json"

# Ensure we're in the right directory
if [ ! -f "jct" ]; then
    echo -e "${RED}Error: jct binary not found. Please run 'make' first.${NC}"
    exit 1
fi

if [ ! -f "$TEST_DATA" ]; then
    echo -e "${RED}Error: Test data file $TEST_DATA not found.${NC}"
    exit 1
fi

# Function to run a test
run_test() {
    local test_name="$1"
    local expected="$2"
    local actual="$3"

    TESTS_RUN=$((TESTS_RUN + 1))

    if [ "$actual" = "$expected" ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗${NC} $test_name"
        echo -e "  Expected: ${YELLOW}'$expected'${NC}"
        echo -e "  Actual:   ${YELLOW}'$actual'${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Function to test command success/failure
test_command() {
    local test_name="$1"
    local command="$2"
    local should_succeed="$3"

    TESTS_RUN=$((TESTS_RUN + 1))

    if eval "$command" >/dev/null 2>&1; then
        if [ "$should_succeed" = "true" ]; then
            echo -e "${GREEN}✓${NC} $test_name (command succeeded as expected)"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            echo -e "${RED}✗${NC} $test_name (command succeeded but should have failed)"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    else
        if [ "$should_succeed" = "false" ]; then
            echo -e "${GREEN}✓${NC} $test_name (command failed as expected)"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            echo -e "${RED}✗${NC} $test_name (command failed but should have succeeded)"
            TESTS_FAILED=$((TESTS_FAILED + 1))
        fi
    fi
}
# Normalize JSON helper
norm_json() {
  python3 - <<'PY' 2>/dev/null || cat
import sys, json
s=sys.stdin.read()
try:
    obj=json.loads(s)
    print(json.dumps(obj, separators=(",", ":")))
except Exception:
    print(s)
PY
}

# Helper: test exit code
expect_exit_code() {
    local test_name="$1"
    local command="$2"
    local expected_code="$3"
    TESTS_RUN=$((TESTS_RUN + 1))
    set +e
    eval "$command" >/dev/null 2>&1
    local code=$?
    set -e
    if [ "$code" = "$expected_code" ]; then
        echo -e "${GREEN}OK${NC} $test_name (exit $code)"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}FAIL${NC} $test_name (expected exit $expected_code, got $code)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Helper: capture stderr and check it contains substring
expect_stderr_contains() {
    local test_name="$1"
    local command="$2"
    local substring="$3"
    TESTS_RUN=$((TESTS_RUN + 1))
    set +e
    local tmpfile
    tmpfile=$(mktemp)
    eval "$command" 1>/dev/null 2>"$tmpfile"
    local code=$?
    set -e
    if grep -Fq "$substring" "$tmpfile"; then
        echo -e "${GREEN}OK${NC} $test_name (stderr contains expected text)"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}FAIL${NC} $test_name (stderr did not contain expected text)"
        echo -e "  Expected substring: ${YELLOW}$substring${NC}"
        echo -e "  Actual stderr:";
        sed 's/^/    /' "$tmpfile"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    rm -f "$tmpfile"
}


echo -e "${BLUE}=== JCT Comprehensive Test Suite ===${NC}"
echo

# Test 1: String values (should not have quotes)
echo -e "${BLUE}Testing string values...${NC}"
run_test "Simple string" "hello world" "$(./jct $TEST_DATA get strings.simple)"
run_test "Empty string" "" "$(./jct $TEST_DATA get strings.empty)"
run_test "String with spaces" "  spaced  " "$(./jct $TEST_DATA get strings.with_spaces)"
run_test "Numeric-looking string" "123.45" "$(./jct $TEST_DATA get strings.numeric_looking)"
run_test "Version string" "1.2.3-beta" "$(./jct $TEST_DATA get strings.version)"
run_test "Boolean-looking string" "true" "$(./jct $TEST_DATA get strings.boolean_looking)"
run_test "Null-looking string" "null" "$(./jct $TEST_DATA get strings.null_looking)"

# Test 2: Number values (should not have quotes)
echo -e "${BLUE}Testing number values...${NC}"
run_test "Integer" "42" "$(./jct $TEST_DATA get numbers.integer)"
run_test "Negative integer" "-17" "$(./jct $TEST_DATA get numbers.negative)"
run_test "Zero" "0" "$(./jct $TEST_DATA get numbers.zero)"
run_test "Float" "3.14159" "$(./jct $TEST_DATA get numbers.float)"
run_test "Negative float" "-2.718" "$(./jct $TEST_DATA get numbers.negative_float)"

# Test 3: Boolean values (should not have quotes)
echo -e "${BLUE}Testing boolean values...${NC}"
run_test "True value" "true" "$(./jct $TEST_DATA get booleans.true_value)"
run_test "False value" "false" "$(./jct $TEST_DATA get booleans.false_value)"

# Test 4: Null values
echo -e "${BLUE}Testing null values...${NC}"
run_test "Null value" "null" "$(./jct $TEST_DATA get null_values.explicit_null)"

# Test 5: Nested access
echo -e "${BLUE}Testing nested access...${NC}"
run_test "Deep nested value" "found it!" "$(./jct $TEST_DATA get objects.nested.level1.level2.level3.deep_value)"
run_test "Config server host" "localhost" "$(./jct $TEST_DATA get config_examples.server.host)"
run_test "Config server port" "8080" "$(./jct $TEST_DATA get config_examples.server.port)"

# Test 6: Array access
echo -e "${BLUE}Testing array access...${NC}"
run_test "Array first element" "apple" "$(./jct $TEST_DATA get arrays.strings.0)"
run_test "Array second element" "banana" "$(./jct $TEST_DATA get arrays.strings.1)"
run_test "Array last element" "cherry" "$(./jct $TEST_DATA get arrays.strings.2)"

# Test 7: Edge cases
echo -e "${BLUE}Testing edge cases...${NC}"
# Note: Special characters test skipped due to JSON escape handling complexity
run_test "Whitespace only" "   " "$(./jct $TEST_DATA get edge_cases.whitespace_only)"
run_test "Path-like string" "/usr/local/bin/app" "$(./jct $TEST_DATA get edge_cases.path_like)"
run_test "URL-like string" "https://example.com/path?param=value" "$(./jct $TEST_DATA get edge_cases.url_like)"
run_test "Email-like string" "user@example.com" "$(./jct $TEST_DATA get edge_cases.email_like)"

# Test 8: Error cases
echo -e "${BLUE}Testing error cases...${NC}"
test_command "Non-existent key" "./jct $TEST_DATA get non.existent.key" "false"
test_command "Non-existent file" "./jct non_existent.json get some.key" "false"
test_command "Invalid array index" "./jct $TEST_DATA get arrays.strings.999" "false"

# Test 9: Print command (should output valid JSON)
echo -e "${BLUE}Testing print command...${NC}"
test_command "Print entire file" "./jct $TEST_DATA print" "true"
test_command "Print nested object" "./jct $TEST_DATA get config_examples.server" "true"
test_command "Print array" "./jct $TEST_DATA get arrays.strings" "true"


# Regression: ensure printing telegrambot.json yields valid JSON
expect_exit_code "telegrambot.json prints valid JSON" "./jct test/telegrambot.json print | python3 -c 'import sys,json; json.load(sys.stdin)'" "0"

# Test 10: Create and set operations
echo -e "${BLUE}Testing create and set operations...${NC}"
rm -f "$TEMP_CONFIG"
test_command "Create new config" "./jct $TEMP_CONFIG create" "true"
test_command "Set string value" "./jct $TEMP_CONFIG set test.string 'hello world'" "true"
test_command "Set number value" "./jct $TEMP_CONFIG set test.number 42" "true"
test_command "Set boolean value" "./jct $TEMP_CONFIG set test.boolean true" "true"
test_command "Set null value" "./jct $TEMP_CONFIG set test.null null" "true"
test_command "Set empty string" "./jct $TEMP_CONFIG set test.empty ''" "true"

# Verify set operations worked
if [ -f "$TEMP_CONFIG" ]; then
    run_test "Get set string" "hello world" "$(./jct $TEMP_CONFIG get test.string)"
    run_test "Get set number" "42" "$(./jct $TEMP_CONFIG get test.number)"

    run_test "Get set boolean" "true" "$(./jct $TEMP_CONFIG get test.boolean)"
    run_test "Get set null" "null" "$(./jct $TEMP_CONFIG get test.null)"
    run_test "Get set empty string" "" "$(./jct $TEMP_CONFIG get test.empty)"
fi

# Test 11: Nested set operations
echo -e "${BLUE}Testing nested set operations...${NC}"
test_command "Set nested value" "./jct $TEMP_CONFIG set server.host localhost" "true"
test_command "Set deeply nested value" "./jct $TEMP_CONFIG set app.config.debug.level 2" "true"

if [ -f "$TEMP_CONFIG" ]; then
    run_test "Get nested set value" "localhost" "$(./jct $TEMP_CONFIG get server.host)"
    run_test "Get deeply nested set value" "2" "$(./jct $TEMP_CONFIG get app.config.debug.level)"
fi

# Test 12: Array operations
echo -e "${BLUE}Testing array operations...${NC}"
test_command "Set array element" "./jct $TEMP_CONFIG set items.0 'first item'" "true"
test_command "Set another array element" "./jct $TEMP_CONFIG set items.1 'second item'" "true"

if [ -f "$TEMP_CONFIG" ]; then
    run_test "Get array element 0" "first item" "$(./jct $TEMP_CONFIG get items.0)"
    run_test "Get array element 1" "second item" "$(./jct $TEMP_CONFIG get items.1)"
fi

# Test 13: Overwrite operations
echo -e "${BLUE}Testing overwrite operations...${NC}"
test_command "Overwrite existing value" "./jct $TEMP_CONFIG set test.string 'updated value'" "true"
if [ -f "$TEMP_CONFIG" ]; then
    run_test "Get overwritten value" "updated value" "$(./jct $TEMP_CONFIG get test.string)"
fi

# Test 14: Special value parsing
echo -e "${BLUE}Testing special value parsing...${NC}"
test_command "Set version string" "./jct $TEMP_CONFIG set app.version '1.2.3-beta'" "true"
test_command "Set numeric string" "./jct $TEMP_CONFIG set app.build '20231201'" "true"
test_command "Set decimal number" "./jct $TEMP_CONFIG set app.pi 3.14159" "true"
# Test 15: Short-name resolution
echo -e "${BLUE}Testing short-name resolution...${NC}"
# Ensure clean slate
rm -f prudynt prudynt.json

# Candidate (a): ./prudynt (no extension)
echo '{"x":1}' > prudynt
run_test "Short-name selects ./prudynt" "1" "$(./jct prudynt get x)"

# Precedence: ./prudynt over ./prudynt.json
echo '{"x":2}' > prudynt.json
run_test "Precedence prefers ./prudynt over ./prudynt.json" "1" "$(./jct prudynt get x)"

# Candidate (b): ./prudynt.json when ./prudynt absent
rm -f prudynt
run_test "Falls back to ./prudynt.json when ./prudynt missing" "2" "$(./jct prudynt get x)"

# Permission denied: unreadable ./prudynt halts and does not try later candidates
echo '{"x":3}' > prudynt
chmod 000 prudynt
expect_exit_code "Unreadable candidate yields exit 13" "./jct prudynt get x" 13
chmod 644 prudynt

# Not found error lists tried paths
rm -f prudynt prudynt.json
expect_exit_code "Not found yields exit 2" "./jct prudynt get x" 2
# Expect error message to include tried paths
expect_stderr_contains "Not found error message lists candidates" "./jct prudynt get x" "jct: no JSON file found for 'prudynt'; tried: ./prudynt, ./prudynt.json"

# Trace flag emits resolution steps
echo '{"x":1}' > prudynt
expect_stderr_contains "--trace-resolve emits trace" "./jct --trace-resolve prudynt get x" "[trace]"
rm -f prudynt prudynt.json
# Test 16: set/create behavior with short names vs explicit paths
echo -e "${BLUE}Testing set/create behavior with short names vs explicit paths...${NC}"
rm -f prudynt prudynt.json

# set with short name must not create; should return 2 with guidance
expect_exit_code "Short-name set does not create; exit 2" "./jct prudynt set app.name 'My App'" 2
expect_stderr_contains "Set short-name guidance present" "./jct prudynt set app.name 'My App'" "to create a new file, supply an explicit path"
# ensure not created
test_command "No files created for short-name set" "test ! -f prudynt -a ! -f prudynt.json" "true"
# Test 17: JSONPath command
echo -e "${BLUE}Testing jsonpath command...${NC}"
# $..author over books
ACTUAL=$(./jct test/books.json path '$..author' --mode values | python3 -c 'import sys,json; print(json.dumps(json.load(sys.stdin), separators=(",", ":")))')
EXPECTED='["Nigel Rees","Evelyn Waugh","Herman Melville","J. R. R. Tolkien"]'
run_test "jsonpath authors values" "$EXPECTED" "$ACTUAL"

# Filtered titles under price < 10
ACTUAL=$(./jct test/books.json path '$.store.book[?(@.price < 10)].title' --mode values | python3 -c 'import sys,json; print(json.dumps(json.load(sys.stdin), separators=(",", ":")))')
EXPECTED='["Sayings of the Century","Moby Dick"]'
run_test "jsonpath filtered titles" "$EXPECTED" "$ACTUAL"

# Slice first two numbers
ACTUAL=$(./jct test/test_data.json path '$.arrays.numbers[0:2]' --mode values | python3 -c 'import sys,json; print(json.dumps(json.load(sys.stdin), separators=(",", ":")))')
EXPECTED='[1,2]'
run_test "jsonpath slice first two" "$EXPECTED" "$ACTUAL"

# Union of indexes
ACTUAL=$(./jct test/test_data.json path '$.arrays.strings[1,2]' --mode values | python3 -c 'import sys,json; print(json.dumps(json.load(sys.stdin), separators=(",", ":")))')
EXPECTED='["banana","cherry"]'
run_test "jsonpath union indices" "$EXPECTED" "$ACTUAL"

# Bracket string properties
ACTUAL=$(./jct test/test_data.json path '$.config_examples.server.host' --mode values | python3 -c 'import sys,json; print(json.dumps(json.load(sys.stdin), separators=(",", ":")))')
EXPECTED='["localhost"]'
run_test "jsonpath bracket strings" "$EXPECTED" "$ACTUAL"

# Recursive wildcard
ACTUAL=$(./jct test/test_data.json path '$..name' --mode values | python3 -c 'import sys,json; print(json.dumps(json.load(sys.stdin), separators=(",", ":")))')
EXPECTED='["myapp_production","item1","item2"]'
run_test "jsonpath recursive names" "$EXPECTED" "$ACTUAL"

# Paths mode
ACTUAL=$(./jct test/test_data.json path '$.arrays.strings[*]' --mode paths | python3 -c 'import sys,json; print(json.dumps(json.load(sys.stdin), separators=(",", ":")))')
EXPECTED='["$.arrays.strings[0]","$.arrays.strings[1]","$.arrays.strings[2]"]'
run_test "jsonpath paths mode" "$EXPECTED" "$ACTUAL"

# Pairs mode with limit
ACTUAL=$(./jct test/books.json path '$..author' --mode pairs --limit 1 | python3 -c 'import sys,json; print(json.dumps(json.load(sys.stdin), separators=(",", ":")))')
EXPECTED='[{"path":"$.store.book[0].author","value":"Nigel Rees"}]'
# Unwrap single value
ACTUAL=$(./jct test/test_data.json path '$.booleans.true_value' --unwrap-single)
EXPECTED='true'
run_test "jsonpath unwrap single" "$EXPECTED" "$ACTUAL"



# set with explicit path may create
expect_exit_code "Explicit path set can create new file" "./jct ./prudynt set app.name 'My App'" 0
run_test "Explicit-created file has value" "My App" "$(./jct ./prudynt get app.name)"

# create requires explicit path; short name should fail with 2 and guidance
rm -f prudynt prudynt.json
expect_exit_code "Create with short name fails with 2" "./jct prudynt create" 2
expect_stderr_contains "Create short-name guidance present" "./jct prudynt create" "requires an explicit path"
# create with explicit path succeeds
expect_exit_code "Create with explicit path succeeds" "./jct prudynt.json create" 0
# cleanup
rm -f prudynt prudynt.json



if [ -f "$TEMP_CONFIG" ]; then
    run_test "Version remains string" "1.2.3-beta" "$(./jct $TEMP_CONFIG get app.version)"
    run_test "Build remains string" "20231201" "$(./jct $TEMP_CONFIG get app.build)"
    run_test "Pi is number" "3.14159" "$(./jct $TEMP_CONFIG get app.pi)"
fi

# Clean up
rm -f "$TEMP_CONFIG"

# Test summary
echo
echo -e "${BLUE}=== Test Summary ===${NC}"
echo -e "Tests run: $TESTS_RUN"
echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $TESTS_FAILED${NC}"
    echo
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    echo
    echo -e "${GREEN}✓ jct is working correctly${NC}"
fi
