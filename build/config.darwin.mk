
# Build options for Linux
CFLAGS += -D__SW_Darwin__

# Install to /usr/local if no alternate is given
PREFIX ?= /usr/local

# Darwin linker uses install_name flag instead of soname
LD_SONAME_FLAG = install_name
