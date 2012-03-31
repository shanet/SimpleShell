# CMPSC 311 Project 7, Makefile
#
# Authors:              Shane Tully and Gage Ames
# Emails:               spt5063@psu.edu and gra5028@psu.edu
#

SRC=pr7.3.c pr7_table.c

BINARY=pr7
CFLAGS=-std=c99 -Wall -Wextra

gcc:
	gcc $(CFLAGS) -D_GNU_SOURCE -O2 -o $(BINARY) $(SRC)

sun:
	c99 -v -D_POSIX_C_SOURCE=200112L -o $(BINARY) $(SRC)

gcc-debug:
	gcc $(CFLAGS) -g -O0 -o $(BINARY) $(SRC)

clean:
	rm $(BINARY)