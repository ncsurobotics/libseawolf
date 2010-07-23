
export CC ?= gcc
export PREFIX ?= /usr/local
export LIB_NAME ?= libseawolf.so
export HUB_NAME ?= seawolf-hub

export CFLAGS = --std=c99 -Wall -Werror -pedantic -Wmissing-prototypes -I$(shell pwd)/include -g -fPIC

export MAJOR_VERSION = 0
export MINOR_VERSION = 1
export REVISION = 0

all: $(LIB_NAME) $(HUB_NAME)

install:
	$(MAKE) -C src install

clean:
	$(MAKE) -C src clean
	rm -rf doc

uninstall:
	rm $(PREFIX)/bin/$(HUB_NAME)
	rm $(PREFIX)/lib/$(LIB_NAME).*
	rm $(PREFIX)/include/seawolf.h
	rm -r $(PREFIX)/include/seawolf

doc:
	doxygen Doxyfile

%:
	$(MAKE) -C src $@

.PHONY: all clean doc install
