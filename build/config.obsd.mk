
# These build options for for OpenBSD

# Install to /usr/local if no alternate is given
PREFIX ?= /usr/local

# OpenBSD does not split their realtime extensions into a separate run time
# library so -lrt is not needed
EXTRA_LDFLAGS =
