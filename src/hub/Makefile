
include ../../build/config.base.mk
include ../../$(CONFIG)

CFLAGS += -I../../include/
LDFLAGS += -L../ -lseawolf

INCLUDES= ../../include/seawolf/*.h ../../include/seawolf.h seawolf_hub.h

SRC= config.c hub.c logging.c netloop.c process.c var.c
OBJ= $(SRC:.c=.o)

all: $(HUB_NAME)

$(HUB_NAME): $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) -o $(HUB_NAME)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ): $(INCLUDES)

clean:
	-rm $(OBJ) $(HUB_NAME) 2> /dev/null

install: $(HUB_NAME)
	install -m 0755 $(HUB_NAME) $(PREFIX)/bin

uninstall:
	-rm $(PREFIX)/bin/$(HUB_NAME)

.PHONY: all clean install uninstall
