# Makefile for Manufacturing Daemon project

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -pthread

# Directories
SRC_DIR = src
INC_DIR = inc
BIN_DIR = bin
OBJ_DIR = obj
LOG_DIR = logs
DATA_DIR = data

# Create directories if they don't exist
$(shell mkdir -p $(BIN_DIR) $(OBJ_DIR) $(LOG_DIR))
$(shell mkdir -p $(DATA_DIR)/upload $(DATA_DIR)/reporting $(DATA_DIR)/backup)

# Default target
all: $(BIN_DIR)/company_daemon $(BIN_DIR)/test_mode

# Link the daemon executable
$(BIN_DIR)/company_daemon: $(OBJ_DIR)/main.o $(OBJ_DIR)/daemon.o $(OBJ_DIR)/company.o
	$(CC) $(LDFLAGS) -o $@ $^

# Link the test mode executable
$(BIN_DIR)/test_mode: $(OBJ_DIR)/test_mode.o $(OBJ_DIR)/daemon.o $(OBJ_DIR)/company.o
	$(CC) $(LDFLAGS) -o $@ $^

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJ_DIR)/*.o $(BIN_DIR)/company_daemon $(BIN_DIR)/test_mode

# Full rebuild
rebuild: clean all

# Run with local paths (for testing)
run: $(BIN_DIR)/company_daemon
	./$(BIN_DIR)/company_daemon

# Run test mode
test: $(BIN_DIR)/test_mode
	./$(BIN_DIR)/test_mode

.PHONY: all clean rebuild run test	