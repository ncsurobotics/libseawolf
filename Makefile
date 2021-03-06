
include build/config.base.mk
include $(CONFIG)

# Ensure that PREFIX is saved as an absolute path
export PREFIX := $(abspath $(PREFIX))

all: $(LIB_FILE) $(HUB_NAME)

$(LIB_FILE):
	cd src && $(MAKE) $@

$(HUB_NAME):
	cd src/hub/ && $(MAKE) $@

pylib:
	cd src/ && $(MAKE) $@

pylib-install:
	cd src && $(MAKE) $@

clean:
	cd src && $(MAKE) $@
	cd src/hub/ && $(MAKE) $@
	-rm -rf doc/html/ 2> /dev/null
	-rm -rf doc/hub/html/ 2> /dev/null

install uninstall:
	cd src && $(MAKE) $@
ifneq ($(strip $(HUB_NAME)),)
	cd src/hub/ && $(MAKE) $@
endif

doc:
	doxygen doc/Doxyfile

doc-hub:
	doxygen doc/hub/Doxyfile

.PHONY: all clean install uninstall doc pylib pylib-install
