# Simple Makefile for Poste Italiane Project

CC       := gcc
CFLAGS   := -Wall -Wextra -Werror -g -std=c99
LDFLAGS  := -lpthread -lrt

SRC       := src
SYS       := $(SRC)/systems
INCLUDE   := include
OBJ       := obj
BIN       := ./bin

# Source files
SRCS := $(SRC)/direttore.c \
        $(SRC)/erogatore_ticket.c \
        $(SRC)/operatore.c \
        $(SRC)/utente.c \
		$(SRC)/new_users.c \
        $(SYS)/msg_queue.c \
        $(SYS)/shared_mem.c \
		$(SYS)/config.c 

# Object files for shared/system modules only
SYSTEM_OBJS := $(OBJ)/systems/msg_queue.o $(OBJ)/systems/shared_mem.o $(OBJ)/systems/config.o

# All object files (for dependency tracking)
ALL_OBJS := $(OBJ)/direttore.o \
            $(OBJ)/erogatore_ticket.o \
            $(OBJ)/operatore.o \
            $(OBJ)/utente.o \
			$(OBJ)/new_users.o \
            $(SYSTEM_OBJS)

# Executables
EXES := $(BIN)/direttore \
        $(BIN)/erogatore_ticket \
        $(BIN)/operatore \
        $(BIN)/utente \
		$(BIN)/new_users

.PHONY: all clean unit test

all: $(EXES)

# Each executable links with its own object file plus shared system objects
$(BIN)/direttore: $(OBJ)/direttore.o $(SYSTEM_OBJS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN)/erogatore_ticket: $(OBJ)/erogatore_ticket.o $(SYSTEM_OBJS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN)/operatore: $(OBJ)/operatore.o $(SYSTEM_OBJS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN)/utente: $(OBJ)/utente.o $(SYSTEM_OBJS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN)/new_users: $(OBJ)/new_users.o $(SYSTEM_OBJS) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile sources to objects with order-only directory dependencies
$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	$(CC) $(CFLAGS) -I$(INCLUDE) -c $< -o $@

$(OBJ)/systems/%.o: $(SYS)/%.c | $(OBJ)/systems
	$(CC) $(CFLAGS) -I$(INCLUDE) -c $< -o $@

# Create directories
$(OBJ) $(OBJ)/systems $(BIN):
	@mkdir -p $@

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

run: clean
	ipcrm -a
	make all
	clear
	$(BIN)/direttore

run_explode: clean
	ipcrm -a
	make all
	clear
	$(BIN)/direttore --config ./configs/config_explode.conf

run_timeout: clean
	ipcrm -a
	make all
	clear
	$(BIN)/direttore --config ./configs/config_timeout.conf

add_users: 
	$(BIN)/new_users --n-new-users $(N)

.PHONY: add_users
#usage: make add_users N=5