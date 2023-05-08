# Makefile for executable adjust

# *****************************************************
# Parameters to control Makefile operation

CC = gcc
CFLAGS = -Wall -pthread

#The option -Wall means to turn on all compiler warnings.
# ****************************************************

all: adzip

adzip: adzip.c
    $(CC) $(CFLAGS) -o adzip: adzip.c


