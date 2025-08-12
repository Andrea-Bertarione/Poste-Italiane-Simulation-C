# Simple Makefile for Poste Italiane Project

CC       := gcc
CFLAGS   := -Wall -Wextra -Werror -g -std=c99
LDFLAGS  := -lpthread -lrt

SRC       := src
SYS       := $(SRC)/systems
INCLUDE   := include
OBJ       := obj
BIN       := bin

# Source files
SRCS := $(SRC)/direttore.c \
        $(SRC)/erogatore_ticket.c \
        $(SRC)/operatore.c \
        $(SRC)/utente.c \
        $(SYS)/msg_queue.c \
        $(SYS)/shared_mem.c

# Object files
OBJS := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(filter $(SRC)/%.c,$(SRCS))) \
        $(patsubst $(SYS)/%.c,$(OBJ)/%.o,$(filter $(SYS)/%.c,$(SRCS)))

# Executables
EXES := $(BIN)/direttore \
        $(BIN)/erogatore_ticket \
        $(BIN)/operatore \
        $(BIN)/utente

.PHONY: all clean unit test

all: $(BIN) $(OBJ) $(EXES)

# Create directories
$(BIN) $(OBJ):
	@mkdir -p $@

# Link each executable
$(BIN)/%: $(OBJ)/%.o $(filter-out $(OBJ)/direttore.o,$(OBJ))  
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Special link for direttore (it only depends on its own object and sys code)
$(BIN)/direttore: $(OBJ)/direttore.o $(OBJ)/msg_queue.o $(OBJ)/shared_mem.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile sources to objects
$(OBJ)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -I$(INCLUDE) -c $< -o $@

$(OBJ)/%.o: $(SYS)/%.c
	$(CC) $(CFLAGS) -I$(INCLUDE) -c $< -o $@

# Unit tests for direttore.c
unit: all $(OBJ)/test_direttore.o
	@mkdir -p $(BIN)
	$(CC) $(CFLAGS) -DUNIT_TEST -I$(INCLUDE) -c src/direttore.c -o $(OBJ)/test_direttore.o
	$(CC) $(CFLAGS) -I$(INCLUDE) tests/test_time.c $(OBJ)/test_direttore.o -o $(BIN)/test_time $(LDFLAGS)
	$(BIN)/test_time
	$(CC) $(CFLAGS) -I$(INCLUDE) tests/test_shm_stats.c $(OBJ)/test_direttore.o -o $(BIN)/test_shm_stats $(LDFLAGS)
	$(BIN)/test_shm_stats

test: unit

clean:
	rm -rf $(OBJ) $(BIN)
