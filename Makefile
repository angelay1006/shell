CC = gcc
CFLAGS = -g3 -Wall -Wextra -Wconversion -Wcast-qual -Wcast-align \
         -Winline -Wfloat-equal -Wnested-externs -pedantic -std=c99 -Werror

# Define -D_GNU_SOURCE only if compiling on Linux
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CFLAGS += -D_GNU_SOURCE
endif


CFLAGS += -DPROMPT

SRC = sh.c jobs.c
OBJ = $(SRC:.c=.o)
TARGET = myshell

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean Command
.PHONY: clean
clean:
	rm -f $(OBJ) $(TARGET)

