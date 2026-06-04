TARGET := walkc
CC := gcc

BASE_CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -g -O0
BASE_LDFLAGS := -ljson-c

SAN_FLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer

CFLAGS := $(BASE_CFLAGS)
LDFLAGS := $(BASE_LDFLAGS)

SRC_DIR := src
BUILD_DIR := build

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

sanitize: CFLAGS += $(SAN_FLAGS)
sanitize: LDFLAGS += $(SAN_FLAGS)
sanitize: clean $(TARGET)

run: $(TARGET)
	./$(TARGET) $(ARGS)

run-sanitize: sanitize
	ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=print_stacktrace=1 ./$(TARGET) $(ARGS)

tags:
	ctags -R $(SRC_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

.PHONY: all clean run sanitize run-sanitize
