# CMPSC 311 Project 7, Makefile
#
# Authors:		Shane Tully and Gage Ames
# Emails:		spt5063@psu.edu and gra5028@psu.edu
#

SRC = pr7.3.c pr7.h pr7_table.[ch]

pr7-gcc: ${SRC}
	gcc -Wall -Wextra -o pr7 ${SRC}

clean:
	rm pr7
