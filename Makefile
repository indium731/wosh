FILE_TO_BUILD = main.c

BUILD_DIR = .build

SRC_DIR = src

CC = gcc

CFLAGS = -Wextra -Wall -g

OUT_FILE = wosh

all:
	$(CC) $(SRC_DIR)/$(FILE_TO_BUILD) -o $(BUILD_DIR)/$(OUT_FILE) $(CFLAGS)

clean:
	rm $(BUILD_DIR)/$(OUT_FILE)

run:
	./$(BUILD_DIR)/$(OUT_FILE)

