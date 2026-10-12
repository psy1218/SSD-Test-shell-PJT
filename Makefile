CC ?= gcc
CPPFLAGS ?= -I.
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -O2
SANITIZER_FLAGS = -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer

BUILD_DIR = build
STORAGE_TEST = $(BUILD_DIR)/test_storage
SANITIZED_TEST = $(BUILD_DIR)/test_storage_sanitize

.PHONY: all test test-sanitize clean

all: $(STORAGE_TEST)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(STORAGE_TEST): ssd.c ssd.h tests/test_storage.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) ssd.c tests/test_storage.c -o $@

$(SANITIZED_TEST): ssd.c ssd.h tests/test_storage.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZER_FLAGS) \
		ssd.c tests/test_storage.c -o $@

test: $(STORAGE_TEST)
	./$(STORAGE_TEST)

test-sanitize: $(SANITIZED_TEST)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1 ./$(SANITIZED_TEST)

clean:
	rm -rf $(BUILD_DIR)
