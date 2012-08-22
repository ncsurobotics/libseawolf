
include ../build/config.base.mk
include ../$(CONFIG)

EXTRA_CFLAGS = -fPIC -I../include/

LDFLAGS += $(EXTRA_LDFLAGS)

INCLUDES= ../include/seawolf/*.h ../include/seawolf.h

SRC = ardcomm.c logging.c main.c notify.c pid.c var.c config.c \
      serial.c stack.c synch.c task.c timer.c util.c dictionary.c \
      list.c queue.c comm.c mem_pool.c
OBJ = $(SRC:.c=.o)

all: $(LIB_FILE)

$(LIB_FILE): $(OBJ)
	$(CC) $(LDFLAGS) $(LIB_LDFLAGS) $(OBJ) -o $(LIB_FILE)
	ln -sf $(LIB_FILE) $(LIB_FILE_BASE)

pylib: $(LIB_FILE)
	cd python && $(PYTHON) setup.py build_ext build

pylib-install: pylib
	cd python && $(PYTHON) setup.py install

.c.o:
	$(CC) $(EXTRA_CFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ): $(INCLUDES)

clean:
	-rm -f $(OBJ) $(LIB_FILE) 2> /dev/null
	-rm -f $(LIB_FILE_BASE)
	-rm -rf python/build/ python/seawolf.py python/seawolf.pyc python/seawolf_wrap.c 2> /dev/null

install: $(LIB_FILE)
	install -m 0755 -d $(PREFIX)/include $(PREFIX)/include/seawolf $(PREFIX)/bin $(PREFIX)/lib
	install -m 0644 ../include/seawolf.h $(PREFIX)/include/
	install -m 0644 ../include/seawolf/* $(PREFIX)/include/seawolf/
	install -m 0644 $(LIB_FILE) $(PREFIX)/lib/$(LIB_FILE)
	ln -sf $(PREFIX)/lib/$(LIB_FILE) $(PREFIX)/lib/$(LIB_FILE_BASE)

uninstall:
	-rm $(PREFIX)/lib/$(LIB_FILE)
	-rm $(PREFIX)/lib/$(LIB_FILE_BASE)
	-rm $(PREFIX)/include/seawolf.h
	-rm -r $(PREFIX)/include/seawolf

.PHONY: all clean install uninstall pylib pylib-install
