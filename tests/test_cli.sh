#!/bin/sh

set -u

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/ssd" >&2
    exit 2
fi

case "$1" in
    /*) SSD_BINARY=$1 ;;
    *) SSD_BINARY="$(pwd)/$1" ;;
esac

if [ ! -x "$SSD_BINARY" ]; then
    echo "ssd binary is not executable: $SSD_BINARY" >&2
    exit 2
fi

TEST_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/ssd-cli-XXXXXX") || exit 1
trap 'rm -rf "$TEST_ROOT"' EXIT HUP INT TERM

total=0
failed=0
command_status=0

start_case()
{
    total=$((total + 1))
    CASE_DIR="$TEST_ROOT/case-$total"
    mkdir "$CASE_DIR" || exit 1
    cd "$CASE_DIR" || exit 1
}

run_ssd()
{
    "$SSD_BINARY" "$@" >stdout.txt 2>stderr.txt
    command_status=$?
}

stdout_is_invalid()
{
    printf 'INVALID COMMAND\n' >expected-stdout.txt
    cmp -s expected-stdout.txt stdout.txt
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

test_write_read()
{
    run_ssd W 3 0x12345678
    [ "$command_status" -eq 0 ] || return 1
    [ ! -s stdout.txt ] || return 1
    [ -f nand.txt ] || return 1
    [ "$(wc -c <nand.txt)" -eq 400 ] || return 1
    [ ! -e result.txt ] || return 1

    run_ssd R 3
    [ "$command_status" -eq 0 ] || return 1
    [ ! -s stdout.txt ] || return 1
    printf '0x12345678\n' >expected-result.txt
    cmp -s expected-result.txt result.txt
}

test_unwritten_read()
{
    run_ssd R 42
    [ "$command_status" -eq 0 ] || return 1
    [ ! -s stdout.txt ] || return 1
    printf '0x00000000\n' >expected-result.txt
    cmp -s expected-result.txt result.txt
}

test_boundary_lbas()
{
    run_ssd W 0 0x01020304
    [ "$command_status" -eq 0 ] || return 1
    run_ssd W 99 0xAABBCCDD
    [ "$command_status" -eq 0 ] || return 1
    run_ssd R 0
    [ "$command_status" -eq 0 ] || return 1
    [ "$(cat result.txt)" = "0x01020304" ] || return 1
    run_ssd R 99
    [ "$command_status" -eq 0 ] || return 1
    [ "$(cat result.txt)" = "0xAABBCCDD" ]
}

test_write_preserves_result()
{
    printf '0xDEADBEEF\n' >result.txt
    cp result.txt expected-result.txt
    run_ssd W 8 0xCAFEBABE
    [ "$command_status" -eq 0 ] || return 1
    [ ! -s stdout.txt ] || return 1
    cmp -s expected-result.txt result.txt
}

test_read_overwrites_result()
{
    printf 'stale data\n' >result.txt
    run_ssd W 5 0xFFFFFFFF
    [ "$command_status" -eq 0 ] || return 1
    run_ssd R 5
    [ "$command_status" -eq 0 ] || return 1
    printf '0xFFFFFFFF\n' >expected-result.txt
    cmp -s expected-result.txt result.txt
}

test_invalid_invocations()
{
    for arguments in "" "X" "R" "R 0 extra" "W 0" \
                     "W 0 0x00000000 extra" "r 0" "w 0 0x00000000"
    do
        # The words are fixed test data and intentional shell splitting is used.
        # shellcheck disable=SC2086
        run_ssd $arguments
        [ "$command_status" -eq 2 ] || return 1
        stdout_is_invalid || return 1
        [ ! -e nand.txt ] || return 1
    done
}

test_invalid_lbas()
{
    for lba in -1 +1 1x 100 999999999999999999999999
    do
        run_ssd W "$lba" 0x00000001
        [ "$command_status" -eq 2 ] || return 1
        stdout_is_invalid || return 1
        [ ! -e nand.txt ] || return 1
    done
}

test_leading_zero_lba()
{
    run_ssd W 003 0x11223344
    [ "$command_status" -eq 0 ] || return 1
    run_ssd R 3
    [ "$command_status" -eq 0 ] || return 1
    [ "$(cat result.txt)" = "0x11223344" ]
}

test_invalid_values()
{
    for value in 0x1234567 0x123456789 0X12345678 0xabcdef12 \
                 0x1234G678 1234567890
    do
        run_ssd W 0 "$value"
        [ "$command_status" -eq 2 ] || return 1
        stdout_is_invalid || return 1
        [ ! -e nand.txt ] || return 1
    done
}

test_corrupt_nand()
{
    printf 'corrupt' >nand.txt
    printf '0xDEADBEEF\n' >result.txt
    cp nand.txt expected-nand.txt
    cp result.txt expected-result.txt

    run_ssd R 0
    [ "$command_status" -eq 4 ] || return 1
    stdout_is_invalid || return 1
    cmp -s expected-nand.txt nand.txt || return 1
    cmp -s expected-result.txt result.txt || return 1

    run_ssd W 0 0x00000000
    [ "$command_status" -eq 4 ] || return 1
    stdout_is_invalid || return 1
    cmp -s expected-nand.txt nand.txt
}

test_nand_io_error()
{
    mkdir nand.txt
    run_ssd R 0
    [ "$command_status" -eq 3 ] || return 1
    stdout_is_invalid || return 1
    [ ! -e result.txt ]
}

test_result_io_error()
{
    run_ssd W 7 0x76543210
    [ "$command_status" -eq 0 ] || return 1
    mkdir result.txt
    run_ssd R 7
    [ "$command_status" -eq 3 ] || return 1
    stdout_is_invalid || return 1
    [ -d result.txt ] || return 1
    [ -z "$(find . -maxdepth 1 -name 'result.txt.tmp.*' -print -quit)" ]
}

test_raw_nand_bytes()
{
    run_ssd W 3 0x12345678
    [ "$command_status" -eq 0 ] || return 1
    actual=$(od -An -tx1 -j12 -N4 nand.txt | tr -d ' \n')
    [ "$actual" = "78563412" ]
}

record "TC-CLI-001 write/read persistence" test_write_read
record "TC-CLI-002 unwritten read" test_unwritten_read
record "TC-CLI-003 boundary LBAs" test_boundary_lbas
record "TC-CLI-004 write preserves result" test_write_preserves_result
record "TC-CLI-005 read overwrites result" test_read_overwrites_result
record "TC-CLI-006 invalid invocations" test_invalid_invocations
record "TC-CLI-007 invalid LBAs" test_invalid_lbas
record "TC-CLI-008 leading-zero LBA" test_leading_zero_lba
record "TC-CLI-009 invalid values" test_invalid_values
record "TC-CLI-010 corrupt NAND" test_corrupt_nand
record "TC-CLI-011 NAND I/O error" test_nand_io_error
record "TC-CLI-012 result I/O error" test_result_io_error
record "TC-CLI-013 raw NAND bytes" test_raw_nand_bytes

echo "SUMMARY total=$total passed=$((total - failed)) failed=$failed"
[ "$failed" -eq 0 ]
