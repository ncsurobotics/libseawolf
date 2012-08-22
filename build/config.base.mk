
# These are all of the default or common configuration options

# Build options
CC ?= gcc
CFLAGS = -std=c99 -Wall -Werror -pedantic -Wmissing-prototypes -g

# Include /usr/local in include and library search paths
CFLAGS += -I/usr/local/include
LDFLAGS += -L/usr/local/lib

# Install prefix
#PREFIX ?= /usr/local

# Library version
MAJOR = 0
MINOR = 1
REV = 0

# Component names
LIB_NAME = seawolf
HUB_NAME = seawolf-hub

# Extra LDFLAGS for some systems
EXTRA_LDFLAGS = 

# Library output file
LIB_FILE = lib$(LIB_NAME).so.$(MAJOR)
LIB_FILE_BASE = lib$(LIB_NAME).so

# Arguments to the compiler for building the shared library
LIB_LDFLAGS = --shared -Wl,-soname,$(LIB_FILE)

# Python executable to use when building Python bindings
PYTHON ?= python

# Attempt to automatically determine the host type
ifndef CONFIG
  HOSTTYPE = $(strip $(shell uname -s))

  ifeq ($(HOSTTYPE), Linux)
    CONFIG = build/config.linux.mk
  else ifeq ($(HOSTTYPE), FreeBSD)
    CONFIG = build/config.freebsd.mk
  else ifeq ($(HOSTTYPE), OpenBSD)
    CONFIG = build/config.obsd.mk
  else ifeq ($(HOSTTYPE), NetBSD)
    CONFIG = build/config.netbsd.mk
  else ifeq ($(HOSTTYPE), Darwin)
    CONFIG = build/config.darwin.mk
  else
    # Fall back to a generic set of options
    CONFIG = build/config.generic.mk
  endif
endif
