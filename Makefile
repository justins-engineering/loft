CC ?= gcc
DIR ?= .
EXTRA_CFLAGS ?=
EXTRA_LDFLAGS ?=
INSTALL ?= cp -pPR

INSTALL_BIN_PATH ?= /usr/bin
INSTALL_INCLUDE_PATH = $(DIR)/include
INSTALL_LIBRARY_PATH = $(DIR)/lib
SRC_PATH = $(DIR)/src
MODULE_PATH = $(DIR)/modules

OPTIMIZATION ?= -O3
WARNINGS = -Wall -Wextra -Wformat -Winline -Werror=format-security -Wstrict-prototypes -Wwrite-strings \
	-Wno-missing-field-initializers -pedantic

CFLAGS ?= -fstack-protector-strong -fanalyzer
LDFLAGS ?= -Wl,-z,relro -Wl,-z,now -Wl,--as-needed -pie

NATIVE ?= 1
ifeq ($(NATIVE), 1)
  arch = -march=native -mtune=native
endif

DEBUG ?= 0
ifeq ($(DEBUG), 1)
EXTRA_CFLAGS += -g -ggdb -fno-omit-frame-pointer
MODULE_CFLAGS = -g -ggdb -fno-omit-frame-pointer
else
EXTRA_CFLAGS += -D_FORTIFY_SOURCE=2
MODULE_CFLAGS = -D_FORTIFY_SOURCE=2 -fstack-protector-strong
endif

MODULE_CFLAGS += $(OPTIMIZATION) $(arch)

linked_libs = -lc -lpthread -lssl -lcrypto -lcurl -lunit -lnaah64 -lhiredis -lhiredis_ssl

CFLAGS += $(OPTIMIZATION) -fPIC $(WARNINGS) $(EXTRA_CFLAGS) $(arch) -I$(INSTALL_INCLUDE_PATH)
LDFLAGS += -L$(DIR) -L$(INSTALL_LIBRARY_PATH) $(linked_libs) $(EXTRA_LDFLAGS)

subdirs= $(addprefix $(SRC_PATH), /db /firmware /json /vzw /curl)

VPATH = $(SRC_PATH) $(subdirs) $(INSTALL_INCLUDE_PATH)

objects = main.o jsmn.o json_helpers.o  request_handler.o firmware_requests.o \
	redis_connect.o helpers.o vzw_connect.o credentials.o

hiredis_headers = $(addprefix hiredis/, hiredis.h async.h read.h sds.h alloc.h sockcompat.h hiredis_ssl.h adapters/*.h)

all: libnaah64.a libhiredis.a libhiredis_ssl.a app

$(INSTALL_INCLUDE_PATH)/jsmn.h:
	$(INSTALL) $(MODULE_PATH)/jsmn/jsmn.h $(INSTALL_INCLUDE_PATH)

$(INSTALL_INCLUDE_PATH)/base64.h:
	$(INSTALL) $(MODULE_PATH)/nibble-and-a-half/base64.h $(INSTALL_INCLUDE_PATH)

$(INSTALL_INCLUDE_PATH)/hiredis/adapters:
	mkdir -p $(INSTALL_INCLUDE_PATH)/hiredis/adapters

$(INSTALL_INCLUDE_PATH)/hiredis/%.h: | $(INSTALL_INCLUDE_PATH)/hiredis/adapters
	$(INSTALL) $(subst $(INSTALL_INCLUDE_PATH), $(MODULE_PATH), $(DIR)/$@) $(@D)

libnaah64.a:
	$(MAKE) -C $(MODULE_PATH)/nibble-and-a-half libnaah64.a CFLAGS='$(MODULE_CFLAGS)'
	$(INSTALL) $(MODULE_PATH)/nibble-and-a-half/libnaah64.a $(INSTALL_LIBRARY_PATH)

libhiredis.a:
	$(MAKE) -C $(MODULE_PATH)/hiredis libhiredis.a DEBUG_FLAGS='' CFLAGS='$(MODULE_CFLAGS)'
	$(INSTALL) $(MODULE_PATH)/hiredis/libhiredis.a $(INSTALL_LIBRARY_PATH)

libhiredis_ssl.a:
	$(MAKE) -C $(MODULE_PATH)/hiredis libhiredis_ssl.a DEBUG_FLAGS='' USE_SSL=1 CFLAGS='$(MODULE_CFLAGS)'
	$(INSTALL) $(MODULE_PATH)/hiredis/libhiredis_ssl.a $(INSTALL_LIBRARY_PATH)

app: config/vzw_secrets.h $(addprefix $(INSTALL_INCLUDE_PATH)/, jsmn.h base64.h $(hiredis_headers)) $(objects)
	$(CC) $(CFLAGS) $(objects) -o $(DIR)/app $(LDFLAGS)
	chmod +x $(DIR)/app

install: app
	cp $(DIR)/app $(INSTALL_BIN_PATH)/loft

# This shouldn't depend on phony targets but they're order-only and it works for now
%.o: %.c | libnaah64.a libhiredis.a libhiredis_ssl.a
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -rf $(DIR)/app $(objects) $(DIR)/lib/*.a $(DIR)/include/jsmn.h \
		$(DIR)/include/base64.h $(DIR)/include/hiredis
	$(MAKE) -C $(MODULE_PATH)/nibble-and-a-half clean
	$(MAKE) -C $(MODULE_PATH)/hiredis clean

.PHONY: clean libnaah64.a libhiredis.a libhiredis_ssl.a install
