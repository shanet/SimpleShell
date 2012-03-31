# CMPSC 311 Project 7, Makefile
#
# Authors:		Shane Tully and Gage Ames
# Emails:		spt5063@psu.edu and gra5028@psu.edu
#

SRC=pr7.3.c pr7.h #pr7_table.[ch]

BINARY=pr7
CFLAGS=-std=c99 -Wall -Wextra


all:
	gcc $(CFLAGS) -O2 -o $(BINARY) $(SRC)

debug:
	gcc $(CFLAGS) -g -O0 -o $(BINARY) $(SRC)

clean:
	rm $(BINARY)
