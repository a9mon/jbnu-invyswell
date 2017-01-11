CC       := gcc
CFLAGS   += -g -Wall -pthread -mrtm
CFLAGS   += -O3
# delete rapl library
CFLAGS   += -I$(LIB)
CPP      := g++
CPPFLAGS += $(CFLAGS)
LD       := g++
LIBS     += -lpthread

LIB := ../lib

STM := ../norec
