CC ?= gcc
CPPFLAGS ?= -I.
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -O2
SANITIZER_FLAGS = -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer

BUILD_DIR = build
SSD_BINARY = ssd
SANITIZED_SSD = $(BUILD_DIR)/ssd_sanitize
STORAGE_TEST = $(BUILD_DIR)/test_storage
SANITIZED_TEST = $(BUILD_DIR)/test_storage_sanitize
CLI_TEST = tests/test_cli.sh

.PHONY: all test test-sanitize clean

all: $(SSD_BINARY) $(STORAGE_TEST)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(SSD_BINARY): ssd.c ssd.h
	$(CC) $(CPPFLAGS) $(CFLAGS) ssd.c -o $@

$(STORAGE_TEST): ssd.c ssd.h tests/test_storage.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSSD_NO_MAIN \
		ssd.c tests/test_storage.c -o $@

$(SANITIZED_SSD): ssd.c ssd.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) ssd.c -o $@

$(SANITIZED_TEST): ssd.c ssd.h tests/test_storage.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) -DSSD_NO_MAIN \
		ssd.c tests/test_storage.c -o $@

test: $(SSD_BINARY) $(STORAGE_TEST)
	./$(STORAGE_TEST)
	sh $(CLI_TEST) ./$(SSD_BINARY)

test-sanitize: $(SANITIZED_SSD) $(SANITIZED_TEST)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1 ./$(SANITIZED_TEST)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1 sh $(CLI_TEST) ./$(SANITIZED_SSD)

clean:
	rm -rf $(BUILD_DIR) $(SSD_BINARY)
