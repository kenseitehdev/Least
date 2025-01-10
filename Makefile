# Compiler
CC = gcc

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
INCLUDE_DIR = include

# Output binary name
TARGET = $(BIN_DIR)/least

# Compiler flags
CFLAGS = -Wall -Wextra -I$(INCLUDE_DIR)

# Linker flags for ncurses
LDFLAGS = -lncurses

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)

# Object files (same name as source files, but with .o in the build directory)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# Default target
all: $(TARGET)

# Linking
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compilation
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create directories if not exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean the build
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)/least

# Phony targets
.PHONY: all clean
