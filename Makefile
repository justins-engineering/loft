CC ?= gcc
DIR ?= .
app_bin_dir ?= /usr/bin
EXTRA_CFLAGS ?=
EXTRA_LDFLAGS ?=

CFLAGS ?= -O3 -fstack-protector-strong -Wall -Wextra -Wformat -Winline -Werror=format-security \
	-D_FORTIFY_SOURCE=2 -march=native -fPIC -I$(DIR)/include -fanalyzer $(EXTRA_CFLAGS)
LDFLAGS ?= -Wl,-z,relro -Wl,-z,now -Wl,--as-needed -pie -L$(DIR) -L$(DIR)/lib -lc -lpthread \
	-lcurl -lunit -lnaah64 -lhiredis $(EXTRA_LDFLAGS)

VPATH = $(DIR)/src:$(DIR)/include

INSTALL?= cp -pPR
INSTALL_INCLUDE_PATH=$(DIR)/include
INSTALL_LIBRARY_PATH=$(DIR)/lib
MODULE_PATH= $(DIR)/modules

objects = main.o jsmn.o json_helpers.o curl_callbacks.o vzw_connect.o request_handler.o \
	firmware_requests.o redis_connect.o

all: jsmn.h base64.h libnaah64.a libhiredis.a app

app: config/vzw_secrets.h $(objects)
	$(CC) $(CFLAGS) $(objects) -o $(DIR)/app $(LDFLAGS)
	chmod +x $(DIR)/app

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

.PHONY: clean jsmn.h base64.h libnaah64.a libhiredis.a install

libnaah64.a:
	$(MAKE) -C $(MODULE_PATH)/nibble-and-a-half libnaah64.a
	$(INSTALL) $(MODULE_PATH)/nibble-and-a-half/libnaah64.a $(INSTALL_LIBRARY_PATH)

base64.h:
	$(INSTALL) $(MODULE_PATH)/nibble-and-a-half/base64.h $(INSTALL_INCLUDE_PATH)

jsmn.h:
	$(INSTALL) $(MODULE_PATH)/jsmn/jsmn.h $(INSTALL_INCLUDE_PATH)

libhiredis.a:
	$(MAKE) -C $(MODULE_PATH)/hiredis static
	mkdir -p $(INSTALL_INCLUDE_PATH)/hiredis/adapters
	cd $(MODULE_PATH)/hiredis && \
		$(INSTALL) hiredis.h async.h read.h sds.h alloc.h sockcompat.h ../../$(INSTALL_INCLUDE_PATH)/hiredis
	$(INSTALL) $(MODULE_PATH)/hiredis/adapters/*.h $(INSTALL_INCLUDE_PATH)/hiredis/adapters
	$(INSTALL) $(MODULE_PATH)/hiredis/libhiredis.a $(INSTALL_LIBRARY_PATH)

install: app
	cp $(DIR)/app $(app_bin_dir)/iots

clean:
	rm -rf $(DIR)/app $(objects) $(DIR)/lib/libnaah64.a $(DIR)/include/jsmn.h \
		$(DIR)/include/base64.h $(DIR)/include/hiredis $(DIR)/lib/libhiredis.a \
		$(MODULE_PATH)/hiredis/include $(MODULE_PATH)/hiredis/lib
	$(MAKE) -C $(MODULE_PATH)/nibble-and-a-half clean
	$(MAKE) -C $(MODULE_PATH)/hiredis clean
