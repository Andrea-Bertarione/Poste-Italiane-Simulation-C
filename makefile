# Makefile for Poste Italiane Project
# Builds all executables into bin/ directory

CC = gcc
CFLAGS = -Wvla -Wextra -Werror -g -std=c99
LDFLAGS = -lpthread -lrt

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

# Source files
DIRETTORE_SRC = $(SRC_DIR)/direttore.c
EROGATORE_SRC = $(SRC_DIR)/erogatore_ticket.c
OPERATORE_SRC = $(SRC_DIR)/operatore.c
UTENTE_SRC = $(SRC_DIR)/utente.c
ADD_USERS_SRC = $(SRC_DIR)/add_users.c

# Object files
DIRETTORE_OBJ = $(OBJ_DIR)/direttore.o
EROGATORE_OBJ = $(OBJ_DIR)/erogatore_ticket.o
OPERATORE_OBJ = $(OBJ_DIR)/operatore.o
UTENTE_OBJ = $(OBJ_DIR)/utente.o
ADD_USERS_OBJ = $(OBJ_DIR)/add_users.o

# Executables in bin/
EXECUTABLES = $(BIN_DIR)/direttore $(BIN_DIR)/erogatore_ticket $(BIN_DIR)/operatore $(BIN_DIR)/utente $(BIN_DIR)/add_users

# Default target
all: directories $(EXECUTABLES)

# Create directories if they don't exist
directories:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

# Build direttore executable
$(BIN_DIR)/direttore: $(DIRETTORE_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built direttore -> $(BIN_DIR)/"

# Build erogatore_ticket executable  
$(BIN_DIR)/erogatore_ticket: $(EROGATORE_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built erogatore_ticket -> $(BIN_DIR)/"

# Build operatore executable
$(BIN_DIR)/operatore: $(OPERATORE_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built operatore -> $(BIN_DIR)/"

# Build utente executable
$(BIN_DIR)/utente: $(UTENTE_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built utente -> $(BIN_DIR)/"

# Build add_users executable
$(BIN_DIR)/add_users: $(ADD_USERS_OBJ) $(COMMON_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built add_users -> $(BIN_DIR)/"

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

# Individual build targets
direttore: $(BIN_DIR)/direttore
erogatore: $(BIN_DIR)/erogatore_ticket
operatore: $(BIN_DIR)/operatore
utente: $(BIN_DIR)/utente
add-users: $(BIN_DIR)/add_users

# Run targets
run: $(BIN_DIR)/direttore
	./$(BIN_DIR)/direttore

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	rm -f *.o *.csv core
	@echo "Cleaned build artifacts"

# Clean IPC resources (in case of crash)
clean-ipc:
	@echo "Cleaning IPC resources..."
	@ipcrm -a 2>/dev/null || true
	@echo "IPC cleanup completed"

# Rebuild everything
rebuild: clean all

# Debug build
debug: CFLAGS += -DDEBUG -O0 -ggdb3
debug: all

# Release build  
release: CFLAGS += -O2 -DNDEBUG
release: CFLAGS := $(filter-out -g,$(CFLAGS))
release: all

# Show what's in bin/
show-bin:
	@echo "Contents of $(BIN_DIR)/:"
	@ls -la $(BIN_DIR)/ 2>/dev/null || echo "$(BIN_DIR)/ is empty or doesn't exist"

# Help
help:
	@echo "Available targets:"
	@echo "  all          - Build all executables in $(BIN_DIR)/"
	@echo "  clean        - Remove build artifacts"  
	@echo "  rebuild      - Clean and build all"
	@echo ""
	@echo "Run targets:"
	@echo "  run          - Run with default config"
	@echo ""
	@echo "Utilities:"
	@echo "  show-bin     - Show contents of bin/ directory"
	@echo "  clean-ipc    - Clean IPC resources"
	@echo "  debug        - Build with debug symbols"
	@echo "  release      - Build optimized version"

# Phony targets
.PHONY: all clean rebuild directories debug release help show-bin clean-ipc
.PHONY: direttore erogatore operatore utente add-users
.PHONY: run run-timeout run-explode test

# Dependencies
$(DIRETTORE_OBJ): $(DIRETTORE_SRC) $(INC_DIR)/poste.h
$(EROGATORE_OBJ): $(EROGATORE_SRC) $(INC_DIR)/poste.h  
$(OPERATORE_OBJ): $(OPERATORE_SRC) $(INC_DIR)/poste.h
$(UTENTE_OBJ): $(UTENTE_SRC) $(INC_DIR)/poste.h
$(ADD_USERS_OBJ): $(ADD_USERS_SRC) $(INC_DIR)/poste.h