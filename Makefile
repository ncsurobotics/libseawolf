
include build/config.base.mk
include $(CONFIG)

# Ensure that PREFIX is saved as an absolute path
export PREFIX := $(abspath $(PREFIX))

all: $(LIB_NAME) $(HUB_NAME)

$(LIB_NAME):
	cd src && $(MAKE) $@

$(HUB_NAME):
	cd src/hub/ && $(MAKE) $@

clean:
	cd src && $(MAKE) $@
	cd src/hub/ && $(MAKE) $@
	-rm -rf doc/html/ 2> /dev/null

install uninstall:
	cd src && $(MAKE) $@
ifneq ($(STRIP $(HUB_NAME)),)
	cd src/hub/ && $(MAKE) $@
endif

doc:
	doxygen doc/Doxyfile

.PHONY: all clean install uninstall doc
