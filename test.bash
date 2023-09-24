#!/usr/bin/env bash

TEST_ID=1
TESTS_FAILED=0

fail() {
    local expected="$1"
    local recieved="$2"

    echo -e "[!] FAIL (Test $TEST_ID):" >&2
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

    echo -e "\nRunning Test $TEST_ID"

    local this
    this="$(echo -e "$commands" | ./build/main 2>&1 | unescape)"

    if [ "$this" != "$expected" ]; then
        fail "$expected" "$this"
    else
        echo "[*] SUCCESS (Test $TEST_ID)"
    fi

    TEST_ID=$((TEST_ID+1))
}

write_til_dot() {
    local result=""
    for line in "$@"; do
        result="$result$line\n"
    done
    result="$result."
    echo "$result"
}

insert_cmd() {
    write_til_dot "i" "$@"
}

append_cmd() {
    write_til_dot "a" "$@"
}

change_cmd() {
    write_til_dot "c" "$@"
}

# INSERT + APPEND
runtest \
    "$(insert_cmd "hello world")\n,p\nq\n" \
    "hello world\n"
runtest \
    "$(append_cmd "hello world")\n,p\nq\n" \
    "hello world\n"
runtest \
    "$(insert_cmd "hello" "world")\n1$(append_cmd "there")\n,p\nq\n" \
    "hello\nthere\nworld\n"

# CHANGE
runtest \
    "$(insert_cmd "hello" "world")\n2$(change_cmd "there")\n,p\nq\n" \
    "hello\nthere\n"
runtest \
    "$(insert_cmd "hello" "world" "how" "are" "you" "today")\n2,5$(change_cmd "what a good day it is")\n,p\nq\n" \
    "hello\nwhat a good day it is\ntoday\n"

# EDIT
runtest \
    "H\ne nonexist\nq\n" \
    "?\nCould not open file.\n"
runtest \
    "e README.md\n,p\nq\n" \
    "$(unescape < "README.md")"

if [ $TESTS_FAILED -eq 0 ]; then
    true
elif [ $TESTS_FAILED -eq 1 ]; then
    echo -e "\n$TESTS_FAILED test failed." >&2
    false
else
    echo -e "\n$TESTS_FAILED tests failed." >&2
    false
fi
