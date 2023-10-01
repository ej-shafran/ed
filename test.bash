#!/usr/bin/env bash

TEST_NAME=""
TESTS_FAILED=0

fail() {
    local expected="$1"
    local recieved="$2"

    printf "\033[0;31m FAIL\033[0m\n" >&2
    printf "\tExpected: '%s'\n" "$expected" >&2
    printf "\tRecieved: '%s'\n" "$recieved" >&2

    TESTS_FAILED=$((TESTS_FAILED+1))
}

unescape() {
     sed 's/$/\\n/' <&0 | tr -d '\n'
}

runtest() {
    local commands="$1"

    printf "\n%-50s" "$TEST_NAME:"

    local recieved
    recieved="$(echo "$commands" | ./build/main 2>&1 | unescape)"
    local expected
    expected="$(echo "$commands" | ed 2>&1 | unescape)"

    if [ "$recieved" != "$expected" ]; then
        fail "$expected" "$recieved"
    else
        printf "\033[0;32m SUCCESS\033[0m\n"
    fi
}

for test_dir in ./tests/*; do
    for dir in "$test_dir"/*; do
        COMMANDS="$dir/commands"
        TEST_NAME="$dir"

        runtest "$(cat "$COMMANDS")"
    done
done

if [ $TESTS_FAILED -eq 0 ]; then
    printf "\nAll tests passed.\n"
    true
elif [ $TESTS_FAILED -eq 1 ]; then
    printf "\n%s test failed.\n" "$TESTS_FAILED" >&2
    false
else
    printf "\n%s tests failed.\n" "$TESTS_FAILED" >&2
    false
fi
