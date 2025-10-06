#!/bin/bash

# Test suite for JSON escape sequence handling
# This test verifies that escape sequences in JSON strings are properly
# handled during parse and serialize operations, preventing the bug where
# backslashes would be doubled on each save operation.

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

TEST_FILE="test/test_escape_temp.json"

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

echo -e "${BLUE}=== JSON Escape Sequence Test Suite ===${NC}"
echo

# Test 1: Escaped quotes should remain stable across multiple saves
echo -e "${BLUE}Test 1: Escaped quotes stability${NC}"
rm -f "$TEST_FILE"
./jct "$TEST_FILE" set a '"a"' 2>/dev/null
FIRST=$(./jct "$TEST_FILE" get a 2>/dev/null)
./jct "$TEST_FILE" set b 1 2>/dev/null
SECOND=$(./jct "$TEST_FILE" get a 2>/dev/null)
./jct "$TEST_FILE" set c 2 2>/dev/null
THIRD=$(./jct "$TEST_FILE" get a 2>/dev/null)
./jct "$TEST_FILE" set d 3 2>/dev/null
FOURTH=$(./jct "$TEST_FILE" get a 2>/dev/null)

run_test "First read of escaped quotes" '"a"' "$FIRST"
run_test "Second read after one save" '"a"' "$SECOND"
run_test "Third read after two saves" '"a"' "$THIRD"
run_test "Fourth read after three saves" '"a"' "$FOURTH"
echo

# Test 2: Backslashes in paths should remain stable
echo -e "${BLUE}Test 2: Backslashes in paths${NC}"
rm -f "$TEST_FILE"
./jct "$TEST_FILE" set path 'C:\Users\test' 2>/dev/null
FIRST=$(./jct "$TEST_FILE" get path 2>/dev/null)
./jct "$TEST_FILE" set other 'value' 2>/dev/null
SECOND=$(./jct "$TEST_FILE" get path 2>/dev/null)
./jct "$TEST_FILE" set another 'value2' 2>/dev/null
THIRD=$(./jct "$TEST_FILE" get path 2>/dev/null)

run_test "First read of path with backslashes" 'C:\Users\test' "$FIRST"
run_test "Second read after one save" 'C:\Users\test' "$SECOND"
run_test "Third read after two saves" 'C:\Users\test' "$THIRD"
echo

# Test 3: Newlines should remain stable
echo -e "${BLUE}Test 3: Newline characters${NC}"
rm -f "$TEST_FILE"
./jct "$TEST_FILE" set text $'line1\nline2' 2>/dev/null
FIRST=$(./jct "$TEST_FILE" get text 2>/dev/null)
./jct "$TEST_FILE" set other 'value' 2>/dev/null
SECOND=$(./jct "$TEST_FILE" get text 2>/dev/null)
./jct "$TEST_FILE" set another 'value2' 2>/dev/null
THIRD=$(./jct "$TEST_FILE" get text 2>/dev/null)

run_test "First read of text with newline" $'line1\nline2' "$FIRST"
run_test "Second read after one save" $'line1\nline2' "$SECOND"
run_test "Third read after two saves" $'line1\nline2' "$THIRD"
echo

# Test 4: Tabs should remain stable
echo -e "${BLUE}Test 4: Tab characters${NC}"
rm -f "$TEST_FILE"
./jct "$TEST_FILE" set text $'col1\tcol2\tcol3' 2>/dev/null
FIRST=$(./jct "$TEST_FILE" get text 2>/dev/null)
./jct "$TEST_FILE" set other 'value' 2>/dev/null
SECOND=$(./jct "$TEST_FILE" get text 2>/dev/null)

run_test "First read of text with tabs" $'col1\tcol2\tcol3' "$FIRST"
run_test "Second read after one save" $'col1\tcol2\tcol3' "$SECOND"
echo

# Test 5: Mixed escape sequences
echo -e "${BLUE}Test 5: Mixed escape sequences${NC}"
rm -f "$TEST_FILE"
./jct "$TEST_FILE" set mixed $'quote: " backslash: \\ newline: \n tab: \t' 2>/dev/null
FIRST=$(./jct "$TEST_FILE" get mixed 2>/dev/null)
./jct "$TEST_FILE" set other 'value' 2>/dev/null
SECOND=$(./jct "$TEST_FILE" get mixed 2>/dev/null)

run_test "First read of mixed escapes" $'quote: " backslash: \\ newline: \n tab: \t' "$FIRST"
run_test "Second read after one save" $'quote: " backslash: \\ newline: \n tab: \t' "$SECOND"
echo

# Test 6: Verify JSON file format is correct
echo -e "${BLUE}Test 6: JSON file format verification${NC}"
rm -f "$TEST_FILE"
./jct "$TEST_FILE" set a '"test"' 2>/dev/null
JSON_CONTENT=$(cat "$TEST_FILE")
# The JSON should contain \" not \\\"
if echo "$JSON_CONTENT" | grep -q '"a": "\\"test\\""'; then
    echo -e "${GREEN}✓${NC} JSON contains correct escape sequence (\\\")"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}✗${NC} JSON does not contain correct escape sequence"
    echo -e "  JSON content: ${YELLOW}$JSON_CONTENT${NC}"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi
TESTS_RUN=$((TESTS_RUN + 1))

# Add another value and verify the first value didn't change
./jct "$TEST_FILE" set b 1 2>/dev/null
JSON_CONTENT=$(cat "$TEST_FILE")
if echo "$JSON_CONTENT" | grep -q '"a": "\\"test\\""'; then
    echo -e "${GREEN}✓${NC} JSON still contains correct escape sequence after second save"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}✗${NC} JSON escape sequence changed after second save"
    echo -e "  JSON content: ${YELLOW}$JSON_CONTENT${NC}"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi
TESTS_RUN=$((TESTS_RUN + 1))
echo

# Clean up
rm -f "$TEST_FILE"

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
    echo -e "${GREEN}✓ Escape sequence handling is working correctly${NC}"
fi

