
# Build options for crosscompiling libseawolf under Linux.

# Build options
CC = bfin-linux-uclibc-gcc
LDFLAGS = -fPIC -lrt

# Install prefix
PREFIX ?= bfin

# Add a suffix to the library name
LIB_NAME = libseawolf-bfin.so
