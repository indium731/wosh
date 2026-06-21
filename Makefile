CC = gcc
CFLAGS = -Wall -Wextra -g

SRC_DIR = src
BUILD_DIR = .build
OUT_FILE = wosh

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

all: $(BUILD_DIR)/$(OUT_FILE)

$(BUILD_DIR)/$(OUT_FILE): $(OBJS)
	$(CC) $(OBJS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

run: all
	./$(BUILD_DIR)/$(OUT_FILE)
