#!/bin/sh

set -u

if [ "$#" -ne 3 ]; then
    echo "usage: $0 BENCHMARK SSD TESTSHELL" >&2
    exit 2
fi

absolute_path()
{
    directory=$(dirname "$1")
    filename=$(basename "$1")
    printf '%s/%s\n' "$(cd "$directory" && pwd)" "$filename"
}

BENCHMARK=$(absolute_path "$1")
SSD=$(absolute_path "$2")
TESTSHELL=$(absolute_path "$3")
TEST_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/benchmark-test-XXXXXX") || exit 1
trap 'rm -rf "$TEST_ROOT"' EXIT HUP INT TERM
cd "$TEST_ROOT" || exit 1

"$BENCHMARK" "$SSD" "$TESTSHELL" 1 performance.csv performance.md \
    >benchmark.out 2>benchmark.err || exit 1

[ "$(wc -l <performance.csv)" -eq 8 ] || exit 1
grep -q '^process_write_100,1,' performance.csv || exit 1
grep -q '^direct_read_100,1,' performance.csv || exit 1
grep -q '^all_automated_tests,1,' performance.csv || exit 1
grep -q '^# Performance Report' performance.md || exit 1
grep -q 'Process/direct Write average ratio:' performance.md || exit 1
[ "$(grep -c 'avg_ms=' benchmark.out)" -eq 7 ] || exit 1

echo "PASS TC-PERF-001 benchmark report"
echo "SUMMARY total=1 passed=1 failed=0"
