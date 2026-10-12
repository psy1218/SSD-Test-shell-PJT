CC ?= gcc
CPPFLAGS ?= -I.
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -O2
SANITIZER_FLAGS = -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer

BUILD_DIR = build
SSD_BINARY = ssd
TESTSHELL_BINARY = testshell
SANITIZED_SSD = $(BUILD_DIR)/ssd_sanitize
SANITIZED_TESTSHELL = $(BUILD_DIR)/testshell_sanitize
SANITIZED_BENCHMARK = $(BUILD_DIR)/benchmark_sanitize
FAULT_BOUNDARY = ssd_fault_boundary
FAULT_OVERWRITE = ssd_fault_overwrite
STORAGE_TEST = $(BUILD_DIR)/test_storage
SANITIZED_TEST = $(BUILD_DIR)/test_storage_sanitize
CLI_TEST = tests/test_cli.sh
SHELL_TEST = tests/test_shell.sh
AUTOMATION_TEST = tests/test_automation.sh
BENCHMARK_TEST = tests/test_benchmark.sh
BENCHMARK_BINARY = $(BUILD_DIR)/benchmark

.PHONY: all test test-sanitize fault-test benchmark analyze quality clean

all: $(SSD_BINARY) $(TESTSHELL_BINARY) $(STORAGE_TEST)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(SSD_BINARY): ssd.c ssd.h
	$(CC) $(CPPFLAGS) $(CFLAGS) ssd.c -o $@

$(TESTSHELL_BINARY): testshell.c
	$(CC) $(CPPFLAGS) $(CFLAGS) testshell.c -o $@

$(STORAGE_TEST): ssd.c ssd.h tests/test_storage.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSSD_NO_MAIN \
		ssd.c tests/test_storage.c -o $@

$(SANITIZED_SSD): ssd.c ssd.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) ssd.c -o $@

$(SANITIZED_TESTSHELL): testshell.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) testshell.c -o $@

$(FAULT_BOUNDARY): ssd.c ssd.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSSD_FAULT_LBA99 ssd.c -o $@

$(FAULT_OVERWRITE): ssd.c ssd.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSSD_FAULT_OVERWRITE ssd.c -o $@

$(SANITIZED_TEST): ssd.c ssd.h tests/test_storage.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) -DSSD_NO_MAIN \
		ssd.c tests/test_storage.c -o $@

$(BENCHMARK_BINARY): ssd.c ssd.h tools/benchmark.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSSD_NO_MAIN \
		ssd.c tools/benchmark.c -o $@

$(SANITIZED_BENCHMARK): ssd.c ssd.h tools/benchmark.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) -DSSD_NO_MAIN \
		ssd.c tools/benchmark.c -o $@

test: $(SSD_BINARY) $(TESTSHELL_BINARY) $(STORAGE_TEST) $(BENCHMARK_BINARY)
	./$(STORAGE_TEST)
	sh $(CLI_TEST) ./$(SSD_BINARY)
	sh $(SHELL_TEST) ./$(TESTSHELL_BINARY) ./$(SSD_BINARY)
	sh $(AUTOMATION_TEST) ./$(TESTSHELL_BINARY) ./$(SSD_BINARY)
	sh $(BENCHMARK_TEST) ./$(BENCHMARK_BINARY) \
		./$(SSD_BINARY) ./$(TESTSHELL_BINARY)

test-sanitize: $(SANITIZED_SSD) $(SANITIZED_TESTSHELL) \
		$(SANITIZED_TEST) $(SANITIZED_BENCHMARK)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1 ./$(SANITIZED_TEST)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1 sh $(CLI_TEST) ./$(SANITIZED_SSD)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1 sh $(SHELL_TEST) \
		./$(SANITIZED_TESTSHELL) ./$(SANITIZED_SSD)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1 sh $(AUTOMATION_TEST) \
		./$(SANITIZED_TESTSHELL) ./$(SANITIZED_SSD)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1 sh $(BENCHMARK_TEST) \
		./$(SANITIZED_BENCHMARK) ./$(SANITIZED_SSD) \
		./$(SANITIZED_TESTSHELL)

fault-test: $(SSD_BINARY) $(TESTSHELL_BINARY) $(FAULT_BOUNDARY) $(FAULT_OVERWRITE)
	sh scripts/run_fault_tests.sh ./$(TESTSHELL_BINARY) ./$(SSD_BINARY) \
		./$(FAULT_BOUNDARY) ./$(FAULT_OVERWRITE)

benchmark: $(SSD_BINARY) $(TESTSHELL_BINARY) $(BENCHMARK_BINARY)
	./$(BENCHMARK_BINARY) "$(abspath $(SSD_BINARY))" \
		"$(abspath $(TESTSHELL_BINARY))" 5 \
		reports/performance_report.csv reports/performance_report.md

analyze: | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -O0 -fanalyzer -Werror \
		-c ssd.c -o $(BUILD_DIR)/ssd_analyze.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -O0 -fanalyzer -Werror \
		-c testshell.c -o $(BUILD_DIR)/testshell_analyze.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -O0 -fanalyzer -Werror -DSSD_NO_MAIN \
		-c tools/benchmark.c -o $(BUILD_DIR)/benchmark_analyze.o

quality: test test-sanitize fault-test analyze

clean:
	rm -rf $(BUILD_DIR) $(SSD_BINARY) $(TESTSHELL_BINARY) \
		$(FAULT_BOUNDARY) $(FAULT_OVERWRITE)
