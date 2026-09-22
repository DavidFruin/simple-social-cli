CC = gcc
CFLAGS = -Wall -Wextra -O2 -Ivendor/include -Ilib

LIB_SRCS = lib/ss_api.c lib/ss_json.c lib/ss_config.c lib/ss_state.c lib/ss_utils.c
LIB_OBJS = $(LIB_SRCS:.c=.o)

CLI_SRCS = cli/main.c cli/output.c
CLI_OBJS = $(CLI_SRCS:.c=.o)

# Static, not shared: the built binary ends up a single self-contained file
# that works wherever it's copied or symlinked (e.g. onto PATH via
# `make install`), with no runtime library search path to get wrong. libcurl
# itself stays dynamic - it's a normal system library, found via its SONAME
# on the default linker search path, not something this project ships.
LIB = lib/libss.a
BIN = simple-social-cli

all: vendor-links $(LIB) $(BIN)

vendor-links:
	@mkdir -p vendor
	@if [ ! -e vendor/libcurl.so ]; then ln -sf /usr/lib/x86_64-linux-gnu/libcurl.so.4.8.0 vendor/libcurl.so; echo "linked vendor/libcurl.so"; fi

$(LIB): $(LIB_OBJS)
	ar rcs $@ $^

lib/%.o: lib/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN): $(CLI_OBJS) $(LIB)
	$(CC) -o $@ $(CLI_OBJS) $(LIB) -Lvendor -lcurl

cli/%.o: cli/%.c
	$(CC) -Wall -Wextra -O2 -Ilib -c -o $@ $<

clean:
	rm -f $(LIB_OBJS) $(CLI_OBJS) $(LIB) $(BIN)

install: $(BIN)
	install -m 755 $(BIN) /usr/local/bin/sscli

uninstall:
	rm -f /usr/local/bin/sscli

.PHONY: all clean install uninstall vendor-links
