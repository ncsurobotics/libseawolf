
# Build options for crosscompiling libseawolf under Linux.
CFLAGS += -D__SW_Linux__ -D__SW_Blackfin__

# Build options
CC = bfin-linux-uclibc-gcc
LDFLAGS = -fPIC -lrt
