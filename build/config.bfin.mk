
# Build options for crosscompiling libseawolf under Linux. This does not build
# the hub by default, but if you have sqlite3 in place for a Blackfin
# environment feel free to remove the line below that disable the building of
# the hub.

# Build options
CC = bfin-linux-uclibc-gcc

# Install prefix
PREFIX ?= bfin

# Remove the hub from the build process
HUB_NAME =
