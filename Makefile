CC = gcc
CFLAGS = -g3 -Wall -Wextra -Wconversion -Wcast-qual -Wcast-align
CFLAGS += -Winline -Wfloat-equal -Wnested-externs
CFLAGS += -pedantic -std=gnu99 -Werror
CFLAGS += -D_GNU_SOURCE

PROMPT = -DPROMPT

SRC = sh.c jobs.c

EXECS = 33sh 33noprompt

# default target
.PHONY: all
all: $(EXECS)

# compile your program, including the -DPROMPT macro
33sh: CFLAGS += $(PROMPT)
33sh: sh_with_prompt.o jobs.o
	$(CC) $(CFLAGS) -o $@ sh_with_prompt.o jobs.o

sh_with_prompt.o: sh.c
	$(CC) $(CFLAGS) -c -o $@ sh.c

jobs.o: jobs.c
	$(CC) $(CFLAGS) -c -o $@ jobs.c

# compile your program without the prompt macro
33noprompt: sh_without_prompt.o jobs.o
	$(CC) $(CFLAGS) -o $@ sh_without_prompt.o jobs.o
sh_without_prompt.o: sh.c
	$(CC) $(CFLAGS) -c -o $@ sh.c

# clean up any executable files that this Makefile has produced
.PHONY: clean
clean:
	rm -f *.o $(EXECS)
	

