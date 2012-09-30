
include ../../build/config.base.mk
include ../../$(CONFIG)

EXTRA_CFLAGS = -I../../include/
LDFLAGS += -L../ -l$(LIB_NAME) -lpthread

INCLUDES= ../../include/seawolf/*.h ../../include/seawolf.h seawolf_hub.h

SRC= config.c hub.c logging.c netio.c netloop.c process.c var.c client.c
OBJ= $(SRC:.c=.o)

all: $(HUB_NAME)

$(HUB_NAME): $(OBJ)
	$(CC) $(OBJ) -o $(HUB_NAME) $(LDFLAGS)

.c.o:
	$(CC) $(EXTRA_CFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ): $(INCLUDES)

clean:
	-rm -f $(OBJ) $(HUB_NAME) 2> /dev/null

install: $(HUB_NAME)
	install -m 0755 $(HUB_NAME) $(PREFIX)/bin

uninstall:
	-rm $(PREFIX)/bin/$(HUB_NAME)

.PHONY: all clean install uninstall
