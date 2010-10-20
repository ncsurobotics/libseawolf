
include ../build/config.base.mk
include ../$(CONFIG)

CFLAGS += -fPIC -I../include/

SONAME = $(LIB_NAME).$(MAJOR)
LDFLAGS += -lpthread --shared -Wl,-soname,$(SONAME)
LDFLAGS += $(EXTRA_LDFLAGS)

INCLUDES= ../include/seawolf/*.h ../include/seawolf.h

SRC = ardcomm.c logging.c main.c notify.c pid.c var.c config.c \
      serial.c stack.c task.c timer.c util.c dictionary.c \
      list.c queue.c comm.c
OBJ = $(SRC:.c=.o)

all: $(LIB_NAME)

$(LIB_NAME): $(OBJ) 
	$(CC) $(LDFLAGS) $(OBJ) -o $(LIB_NAME)

pylib: $(LIB_NAME)
	cd python && $(PYTHON) setup.py build_ext build

pylib-install: pylib
	cd python && $(PYTHON) setup.py install

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ): $(INCLUDES)

clean:
	-rm -f $(OBJ) $(LIB_NAME) 2> /dev/null
	-rm -rf python/build/ python/seawolf.py python/seawolf.pyc python/seawolf_wrap.c 2> /dev/null

install: $(LIB_NAME)
	install -m 0755 -d $(PREFIX)/include $(PREFIX)/include/seawolf $(PREFIX)/bin $(PREFIX)/lib
	install -m 0644 $(LIB_NAME) $(PREFIX)/lib/$(LIB_NAME).$(MAJOR).$(MINOR).$(REV)
	install -m 0644 ../include/seawolf.h $(PREFIX)/include/
	install -m 0644 ../include/seawolf/* $(PREFIX)/include/seawolf/
	ln -sf $(PREFIX)/lib/$(LIB_NAME).$(MAJOR).$(MINOR).$(REV) $(PREFIX)/lib/$(LIB_NAME).$(MAJOR)
	ln -sf $(PREFIX)/lib/$(LIB_NAME).$(MAJOR).$(MINOR).$(REV) $(PREFIX)/lib/$(LIB_NAME)

uninstall:
	-rm $(PREFIX)/lib/$(LIB_NAME)*
	-rm $(PREFIX)/include/seawolf.h
	-rm -r $(PREFIX)/include/seawolf

.PHONY: all clean install uninstall pylib pylib-install
