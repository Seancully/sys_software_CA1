# Makefile for Manufacturing Daemon project

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -O2
LDFLAGS = -pthread

# Directories
SRC_DIR = src
INC_DIR = inc
BIN_DIR = bin
OBJ_DIR = obj
LOG_DIR = logs
DATA_DIR = data

# Define directories for installation
INSTALL_BIN = /usr/sbin
INSTALL_INIT = /etc/init.d
VAR_DIR = /var/company

# Source and object files
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))
DEPS = $(wildcard $(INC_DIR)/*.h)
TARGET = $(BIN_DIR)/company_daemon

# Create directories if they don't exist
$(shell mkdir -p $(BIN_DIR) $(OBJ_DIR) $(LOG_DIR))
$(shell mkdir -p $(DATA_DIR)/upload $(DATA_DIR)/reporting $(DATA_DIR)/backup)

# Default target
all: $(TARGET)

# Link the executable
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(DEPS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET)

# Full rebuild
rebuild: clean all

# Install the daemon
install: $(TARGET)
	mkdir -p $(VAR_DIR)/logs $(VAR_DIR)/upload $(VAR_DIR)/reporting $(VAR_DIR)/backup
	install -m 755 $(TARGET) $(INSTALL_BIN)/
	install -m 755 init.d/company_daemon $(INSTALL_INIT)/company_daemon
	chmod 755 $(VAR_DIR)/upload
	chmod 755 $(VAR_DIR)/reporting
	chmod 755 $(VAR_DIR)/backup
	chmod 755 $(VAR_DIR)/logs
	@echo "Daemon installed successfully. Use 'sudo service company_daemon start' to start it."

# Uninstall the daemon
uninstall:
	rm -f $(INSTALL_BIN)/company_daemon
	rm -f $(INSTALL_INIT)/company_daemon
	@echo "Daemon uninstalled. Data directories have been preserved."
	@echo "To completely remove all data, manually delete $(VAR_DIR)."

# Run with local paths (for testing)
run: $(TARGET)
	./$(TARGET)

# Show help
help:
	@echo "Manufacturing Daemon Makefile"
	@echo "--------------------------"
	@echo "Targets:"
	@echo "  all      - Build the daemon (default)"
	@echo "  clean    - Remove build artifacts"
	@echo "  rebuild  - Perform a clean build"
	@echo "  install  - Install the daemon to system directories"
	@echo "  uninstall- Remove the daemon from system directories"
	@echo "  run      - Run the daemon with local paths (for testing)"
	@echo "  help     - Show this help message"

# Set .PHONY for targets that don't create files
.PHONY: all clean rebuild install uninstall run help