#!/bin/sh

set -u

if [ "$#" -ne 4 ]; then
    echo "usage: $0 TESTSHELL NORMAL_SSD BOUNDARY_SSD OVERWRITE_SSD" >&2
    exit 2
fi

absolute_path()
{
    directory=$(dirname "$1")
    filename=$(basename "$1")
    printf '%s/%s\n' "$(cd "$directory" && pwd)" "$filename"
}

TESTSHELL=$(absolute_path "$1")
NORMAL_SSD=$(absolute_path "$2")
BOUNDARY_SSD=$(absolute_path "$3")
OVERWRITE_SSD=$(absolute_path "$4")
TEST_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/fault-tests-XXXXXX") || exit 1
trap 'rm -rf "$TEST_ROOT"' EXIT HUP INT TERM

total=0
failed=0
detected_mutations=0

start_case()
{
    total=$((total + 1))
    CASE_DIR="$TEST_ROOT/case-$total"
    mkdir "$CASE_DIR" || exit 1
    cd "$CASE_DIR" || exit 1
}

record()
{
    name=$1
    shift
    start_case
    if "$@"; then
        echo "PASS $name"
    else
        echo "FAIL $name"
        failed=$((failed + 1))
    fi
}

run_and_capture()
{
    ssd_path=$1
    shift
    "$TESTSHELL" --ssd-path "$ssd_path" "$@" >test.out 2>test.err
    run_status=$?
}

test_normal_baseline()
{
    run_and_capture "$NORMAL_SSD" --run-tests
    [ "$run_status" -eq 0 ] || return 1
    grep -q '^SUMMARY total=9 pass=9 fail=0 error=0 ' test.out
}

test_boundary_specific()
{
    run_and_capture "$BOUNDARY_SSD" --run-test TC-B002
    [ "$run_status" -eq 1 ] || return 1
    grep -q '^\[FAIL\] TC-B002 ' test.out || return 1
    grep -q 'reason: DATA_MISMATCH' test.out || return 1
    detected_mutations=$((detected_mutations + 1))
    return 0
}

test_boundary_suite()
{
    run_and_capture "$BOUNDARY_SSD" --run-tests
    [ "$run_status" -eq 1 ] || return 1
    grep -q '^\[FAIL\] TC-B002 ' test.out || return 1
    grep -q '^\[FAIL\] TC-S001 ' test.out
}

test_overwrite_specific()
{
    run_and_capture "$OVERWRITE_SSD" --run-test TC-F003
    [ "$run_status" -eq 1 ] || return 1
    grep -q '^\[FAIL\] TC-F003 ' test.out || return 1
    grep -q 'reason: DATA_MISMATCH' test.out || return 1
    detected_mutations=$((detected_mutations + 1))
    return 0
}

test_overwrite_testapp2()
{
    run_and_capture "$OVERWRITE_SSD" --run-test TC-S002
    [ "$run_status" -eq 1 ] || return 1
    grep -q '^\[FAIL\] TC-S002 ' test.out || return 1
    grep -q 'reason: DATA_MISMATCH' test.out
}

test_overwrite_suite()
{
    run_and_capture "$OVERWRITE_SSD" --run-tests
    [ "$run_status" -eq 1 ] || return 1
    grep -q '^\[FAIL\] TC-F003 ' test.out || return 1
    grep -q '^\[FAIL\] TC-S002 ' test.out
}

test_normal_regression()
{
    run_and_capture "$NORMAL_SSD" --run-tests
    [ "$run_status" -eq 0 ] || return 1
    grep -q '^SUMMARY total=9 pass=9 fail=0 error=0 ' test.out
}

record "TC-M000 normal baseline" test_normal_baseline
record "TC-M001 LBA 99 mutation detected" test_boundary_specific
record "TC-M002 boundary suite detection" test_boundary_suite
record "TC-M003 overwrite mutation detected" test_overwrite_specific
record "TC-M004 TestApp2 detects overwrite" test_overwrite_testapp2
record "TC-M005 overwrite suite detection" test_overwrite_suite
record "TC-M006 normal regression" test_normal_regression

mutation_score=$((detected_mutations * 100 / 2))
echo "MUTATION_SCORE detected=$detected_mutations total=2 score=${mutation_score}%"
echo "SUMMARY total=$total passed=$((total - failed)) failed=$failed"

[ "$failed" -eq 0 ] && [ "$detected_mutations" -eq 2 ]
