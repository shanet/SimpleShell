# CMPSC 311 Project 7, Makefile
#
# Authors:              Shane Tully and Gage Ames
# Emails:               spt5063@psu.edu and gra5028@psu.edu
#

SRC=pr7.3.c pr7_table.c

BINARY=pr7
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
