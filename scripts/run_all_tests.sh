#!/bin/sh

set -eu

project_directory=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_directory"

make clean
make
make test
make test-sanitize
make fault-test
make analyze
./testshell --ssd-path ./ssd --run-tests

echo "QUALITY GATE PASS"
