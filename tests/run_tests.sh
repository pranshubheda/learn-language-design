#!/usr/bin/env bash
# Test runner.
#   1. Every examples/X.pb must print exactly what's in examples/X.expected.
#   2. Every tests/errors/X.pb must FAIL, and its output must contain the text
#      after "// expect: " on its first line.
set -u
cd "$(dirname "$0")/.."
BIN=./pebble
pass=0
fail=0

for src in examples/*.pb; do
    expected="${src%.pb}.expected"
    [ -f "$expected" ] || continue
    actual=$("$BIN" "$src" 2>&1)
    if [ "$actual" == "$(cat "$expected")" ]; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        echo "FAIL $src (output differs; < actual, > expected)"
        diff <(echo "$actual") "$expected"
    fi
done

for src in tests/errors/*.pb; do
    want=$(sed -n 's|^// expect: ||p' "$src" | head -1)
    actual=$("$BIN" "$src" 2>&1)
    status=$?
    if [ $status -ne 0 ] && grep -qF -- "$want" <<< "$actual"; then
        pass=$((pass + 1))
    else
        fail=$((fail + 1))
        echo "FAIL $src"
        echo "  wanted an error containing: $want"
        echo "  got (exit $status):"
        echo "$actual" | sed 's/^/    /'
    fi
done

echo "$pass passed, $fail failed"
[ $fail -eq 0 ]
