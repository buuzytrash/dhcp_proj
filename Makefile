CC = gcc 
CFLAGS = -Wall -Wextra

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
BINS = $(addprefix $(BIN_DIR)/, client)

all: $(BINS)

# $(BIN_DIR)/server: $(OBJ_DIR)/server.o | $(BIN_DIR)
# 	$(CC) $(CFLAGS) -o $@ $<

$(BIN_DIR)/client: $(OBJ_DIR)/client.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

# run-server: $(BIN_DIR)/server
# 	./$(BIN_DIR)/server

run-client: $(BIN_DIR)/client
	./$(BIN_DIR)/client

.PHONY: all clean run-client