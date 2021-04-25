# Makefile for IOS project 2
# Author: Vojtech Kalis, xkalis03@fit.vutbr.cz

CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic -pthread
CC=gcc

all: proj2
proj2: proj2.c
	$(CC) $(CFLAGS) proj2.c -o proj2 -lrt