CC = gcc
CFLAGS = -std=c2x -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-unused-parameter -fno-asm
INCLUDES = -Iinclude
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

all: shell.out
shell.out: $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJ) -o shell.out
src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
clean:
	rm -f $(OBJ) shell.out