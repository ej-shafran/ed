#!/usr/bin/env bash

TEST_NAME=""
TESTS_FAILED=0

fail() {
    local expected="$1"
    local recieved="$2"

    echo -e "[!] FAIL:" >&2
    echo -en "\t" >&2
    echo "Expected: '$expected'" >&2
    echo -en "\t" >&2
    echo "Recieved: '$recieved'" >&2

    TESTS_FAILED=$((TESTS_FAILED+1))
}

unescape() {
     sed 's/$/\\n/' <&0 | tr -d '\n'
}

runtest() {
    local commands="$1"
    local expected="$2"

    echo -e "\nTest \`$TEST_NAME\`"

    local this
    this="$(echo -e "$commands" | ./build/main 2>&1 | unescape)"

    if [ "$this" != "$expected" ]; then
        fail "$expected" "$this"
    else
        echo "[*] SUCCESS"
    fi
}

for dir in ./tests/*; do
    COMMANDS="$dir/commands"
    EXPECTED="$dir/expected"
    TEST_NAME="$(basename "$dir")"

    runtest "$(cat "$COMMANDS")" "$(unescape < "$EXPECTED")"
done

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\nAll tests passed."
    true
elif [ $TESTS_FAILED -eq 1 ]; then
    echo -e "\n$TESTS_FAILED test failed." >&2
    false
else
    echo -e "\n$TESTS_FAILED tests failed." >&2
    false
fi
