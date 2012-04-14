# SimpleShell
#
# Authors: Shane Tully and Gage Ames

SRC=ss.c ss_table.c

BINARY=simpleshell
CFLAGS=-std=c99 -Wall -Wextra
MACROS=-D_GNU_SOURCE
LIBS=-lreadline

gcc:
	gcc $(CFLAGS) $(MACROS) -O2 -o $(BINARY) $(SRC) $(LIBS)

sun:
	c99 -v -D_POSIX_C_SOURCE=200112L -o $(BINARY) $(SRC) $(LIBS)

gcc-debug:
	gcc $(CFLAGS) $(MACROS) -g -O0 -o $(BINARY) $(SRC) $(LIBS)

clean:
	rm $(BINARY)
