#!/bin/sh

set -u

if [ "$#" -ne 2 ]; then
    echo "usage: $0 /path/to/testshell /path/to/ssd" >&2
    exit 2
fi

absolute_path()
{
    directory=$(dirname "$1")
    filename=$(basename "$1")
    printf '%s/%s\n' "$(cd "$directory" && pwd)" "$filename"
}

TESTSHELL=$(absolute_path "$1")
SSD=$(absolute_path "$2")
TEST_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/automation-XXXXXX") || exit 1
trap 'rm -rf "$TEST_ROOT"' EXIT HUP INT TERM

total=0
failed=0
test_status=0

start_case()
{
    total=$((total + 1))
    CASE_DIR="$TEST_ROOT/case-$total"
    mkdir "$CASE_DIR" || exit 1
    cd "$CASE_DIR" || exit 1
}

run_testshell()
{
    "$TESTSHELL" --ssd-path "$SSD" "$@" >stdout.txt 2>stderr.txt
    test_status=$?
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

test_run_all()
{
    run_testshell --run-tests
    [ "$test_status" -eq 0 ] || return 1
    [ "$(grep -c '^\[PASS\]' stdout.txt)" -eq 9 ] || return 1
    grep -q '^SUMMARY total=9 pass=9 fail=0 error=0 ' stdout.txt || return 1
    [ -f logs/test_result.log ] || return 1
    [ -f logs/test_summary.txt ] || return 1
    [ -f reports/test_report.csv ] || return 1
    [ -f reports/test_report.md ] || return 1
    [ "$(wc -l <reports/test_report.csv)" -eq 10 ]
}

test_run_single()
{
    run_testshell --run-test TC-F003
    [ "$test_status" -eq 0 ] || return 1
    grep -q '^\[PASS\] TC-F003 ' stdout.txt || return 1
    grep -q '^SUMMARY total=1 pass=1 fail=0 error=0 ' stdout.txt
}

test_unknown_id()
{
    run_testshell --run-test TC-UNKNOWN
    [ "$test_status" -eq 2 ] || return 1
    grep -qx 'UNKNOWN TEST: TC-UNKNOWN' stdout.txt
}

test_interactive_test_commands()
{
    printf 'testapp1\ntestapp2\ntestcase TC-B002\ntestsummary\nexit\n' \
        | "$TESTSHELL" --ssd-path "$SSD" >stdout.txt 2>stderr.txt
    test_status=$?
    [ "$test_status" -eq 0 ] || return 1
    [ "$(grep -c '^\[PASS\]' stdout.txt)" -eq 3 ] || return 1
    [ "$(grep -c '^SUMMARY ' stdout.txt)" -eq 4 ]
}

test_user_data_isolation()
{
    "$SSD" W 12 0xCAFEBABE >setup-out.txt 2>setup-err.txt || return 1
    cp nand.txt nand-before.txt
    run_testshell --run-test TC-S001
    [ "$test_status" -eq 0 ] || return 1
    cmp -s nand-before.txt nand.txt || return 1
    "$SSD" R 12 >read-out.txt 2>read-err.txt || return 1
    grep -qx '0xCAFEBABE' result.txt
}

test_repeated_run()
{
    run_testshell --run-test TC-F001
    [ "$test_status" -eq 0 ] || return 1
    first_summary=$(grep '^SUMMARY ' stdout.txt)
    run_testshell --run-test TC-F001
    [ "$test_status" -eq 0 ] || return 1
    second_summary=$(grep '^SUMMARY ' stdout.txt)
    echo "$first_summary" | grep -q 'total=1 pass=1 fail=0 error=0' || return 1
    echo "$second_summary" | grep -q 'total=1 pass=1 fail=0 error=0'
}

test_missing_ssd_is_error()
{
    "$TESTSHELL" --ssd-path ./missing --run-test TC-F001 \
        >stdout.txt 2>stderr.txt
    test_status=$?
    [ "$test_status" -eq 2 ] || return 1
    grep -q '^\[ERROR\] TC-F001 ' stdout.txt || return 1
    grep -q '^SUMMARY total=1 pass=0 fail=0 error=1 ' stdout.txt
}

test_report_fields()
{
    run_testshell --run-test TC-B002
    [ "$test_status" -eq 0 ] || return 1
    head -1 reports/test_report.csv \
        | grep -q 'test_id,requirement_id,name,status,duration_ms' || return 1
    grep -q '^\[REQUIREMENT\] STOR-008$' logs/test_result.log || return 1
    grep -q '^| TC-B002 | STOR-008 |' reports/test_report.md
}

record "TC-AUTO-001 run all" test_run_all
record "TC-AUTO-002 run single" test_run_single
record "TC-AUTO-003 unknown test ID" test_unknown_id
record "TC-AUTO-004 interactive test commands" test_interactive_test_commands
record "TC-AUTO-005 user data isolation" test_user_data_isolation
record "TC-AUTO-006 repeated execution" test_repeated_run
record "TC-AUTO-007 environment error exit" test_missing_ssd_is_error
record "TC-AUTO-008 report fields" test_report_fields

echo "SUMMARY total=$total passed=$((total - failed)) failed=$failed"
[ "$failed" -eq 0 ]
