
# Build options for NetBSD systems

# Disable warnings about character subscripts caused by NetBSD's toupper and
# tolower macros in ctype.h
CFLAGS += -Wno-char-subscripts

# Install to /usr/local if no alternate is given
PREFIX ?= /usr/local

# NetBSD has POSIX real time extensions in a separate library
EXTRA_LDFLAGS = -lrt
