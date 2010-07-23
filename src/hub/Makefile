
HUB_LDFLAGS= -L../ -lseawolf

# Sqlite flags
SQLITE_LDFLAGS=$(shell pkg-config --libs sqlite3)
SQLITE_CFLAGS=$(shell pkg-config --cflags sqlite3)

INCLUDES= ../../include/seawolf/* ../../include/seawolf.h seawolf_hub.h

HUB_SRC= config.c db.c hub.c logging.c netloop.c process.c var.c
HUB_OBJECTS= $(HUB_SRC:.c=.o)

all: $(HUB_NAME)

$(HUB_NAME): $(HUB_OBJECTS)
	$(CC) $(HUB_LDFLAGS) $(SQLITE_LDFLAGS) $(HUB_OBJECTS) -o $(HUB_NAME)

%.o: %.c $(INCLUDES)
	$(CC) $(CFLAGS) $(SQLITE_CFLAGS) -c $< -o $@

install: $(HUB_NAME)
	install -m 0755 $(HUB_NAME) $(PREFIX)/bin

clean:
	@for _f in $(HUB_OBJECTS) $(HUB_NAME); do \
	  if [ -f $$_f ]; then \
	    echo rm $$_f; \
	    rm $$_f; \
	  fi; \
	done;

.PHONY: all clean
