
# Build options for Linux
CFLAGS += -D__SW_Darwin__

# Install to /usr/local if no alternate is given
PREFIX ?= /usr/local

# Library output file
LIB_FILE = lib$(LIB_NAME).$(MAJOR).dylib
LIB_FILE_BASE = lib$(LIB_NAME).dylib

# Arguments to the compiler for building the shared library
LIB_LDFLAGS = -dynamiclib -Wl,-headerpad_max_install_names,-undefined,dynamic_lookup,-compatibility_version,$(MAJOR).0,-current_version,$(MAJOR).$(MINOR),-install_name,$(PREFIX)/lib/$(LIB_FILE)
