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
TEST_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/testshell-XXXXXX") || exit 1
trap 'rm -rf "$TEST_ROOT"' EXIT HUP INT TERM

total=0
failed=0
shell_status=0

start_case()
{
    total=$((total + 1))
    CASE_DIR="$TEST_ROOT/case-$total"
    mkdir "$CASE_DIR" || exit 1
    cd "$CASE_DIR" || exit 1
}

run_shell_input()
{
    input=$1
    printf '%s' "$input" | "$TESTSHELL" --ssd-path "$SSD" \
        >stdout.txt 2>stderr.txt
    shell_status=$?
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
    run_shell_input 'write 3 0x12345678
read 3
exit
'
    [ "$shell_status" -eq 0 ] || return 1
    printf '0x12345678\n' >expected.txt
    cmp -s expected.txt stdout.txt
}

test_fullwrite_fullread()
{
    run_shell_input 'fullwrite 0xA5A5A5A5
fullread
exit
'
    [ "$shell_status" -eq 0 ] || return 1
    [ "$(wc -l <stdout.txt)" -eq 100 ] || return 1
    head -1 stdout.txt | grep -qx 'LBA 00: 0xA5A5A5A5' || return 1
    tail -1 stdout.txt | grep -qx 'LBA 99: 0xA5A5A5A5'
}

test_help()
{
    run_shell_input 'help
exit
'
    [ "$shell_status" -eq 0 ] || return 1
    [ "$(wc -l <stdout.txt)" -eq 6 ] || return 1
    grep -q '^write ' stdout.txt || return 1
    grep -q '^fullread ' stdout.txt || return 1
    grep -q '^exit ' stdout.txt
}

test_invalid_commands_continue()
{
    run_shell_input '
unknown
write 0
read 0 extra
write 0 0xabcdef12
write 0 0x00000001
read 0
exit
'
    [ "$shell_status" -eq 0 ] || return 1
    [ "$(grep -c '^INVALID COMMAND$' stdout.txt)" -eq 5 ] || return 1
    tail -1 stdout.txt | grep -qx '0x00000001'
}

test_missing_ssd()
{
    printf 'read 0\nexit\n' | "$TESTSHELL" --ssd-path ./missing-ssd \
        >stdout.txt 2>stderr.txt
    shell_status=$?
    [ "$shell_status" -eq 0 ] || return 1
    grep -qx 'INVALID COMMAND' stdout.txt || return 1
    grep -q 'execv failed' stderr.txt
}

test_persistence_across_shells()
{
    run_shell_input 'write 99 0xDEADBEEF
exit
'
    [ "$shell_status" -eq 0 ] || return 1
    run_shell_input 'read 99
exit
'
    [ "$shell_status" -eq 0 ] || return 1
    grep -qx '0xDEADBEEF' stdout.txt
}

test_eof_exit()
{
    run_shell_input 'write 1 0x01020304
'
    [ "$shell_status" -eq 0 ] || return 1
    [ ! -s stdout.txt ] || return 1
    [ -f nand.txt ]
}

test_long_input_recovery()
{
    long_line=$(printf 'x%.0s' $(seq 1 300))
    run_shell_input "$long_line
write 2 0x22222222
read 2
exit
"
    [ "$shell_status" -eq 0 ] || return 1
    head -1 stdout.txt | grep -qx 'INVALID COMMAND' || return 1
    tail -1 stdout.txt | grep -qx '0x22222222'
}

test_exact_buffer_boundary()
{
    boundary_line=$(printf 'x%.0s' $(seq 1 255))
    run_shell_input "$boundary_line
write 4 0x44444444
read 4
exit
"
    [ "$shell_status" -eq 0 ] || return 1
    head -1 stdout.txt | grep -qx 'INVALID COMMAND' || return 1
    tail -1 stdout.txt | grep -qx '0x44444444'
}

test_argument_overflow()
{
    run_shell_input 'write 0 0x00000000 extra token
help extra
exit extra
exit
'
    [ "$shell_status" -eq 0 ] || return 1
    [ "$(grep -c '^INVALID COMMAND$' stdout.txt)" -eq 3 ]
}

test_nand_io_failure()
{
    mkdir nand.txt
    run_shell_input 'read 0
exit
'
    [ "$shell_status" -eq 0 ] || return 1
    grep -qx 'INVALID COMMAND' stdout.txt
}

test_invalid_cli_options()
{
    "$TESTSHELL" --unknown >stdout.txt 2>stderr.txt
    shell_status=$?
    [ "$shell_status" -eq 2 ] || return 1
    grep -qx 'INVALID COMMAND' stdout.txt
}

record "TC-SH-001 write/read" test_write_read
record "TC-SH-002 fullwrite/fullread" test_fullwrite_fullread
record "TC-SH-003 help" test_help
record "TC-SH-004 invalid commands recover" test_invalid_commands_continue
record "TC-SH-005 missing SSD process" test_missing_ssd
record "TC-SH-006 persistence across shells" test_persistence_across_shells
record "TC-SH-007 EOF exit" test_eof_exit
record "TC-SH-008 long input recovery" test_long_input_recovery
record "TC-SH-009 exact input-buffer boundary" test_exact_buffer_boundary
record "TC-SH-010 extra arguments" test_argument_overflow
record "TC-SH-011 NAND I/O failure" test_nand_io_failure
record "TC-SH-012 invalid CLI options" test_invalid_cli_options

echo "SUMMARY total=$total passed=$((total - failed)) failed=$failed"
[ "$failed" -eq 0 ]
