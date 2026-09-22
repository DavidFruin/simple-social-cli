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

# The headers are vendored, so only the runtime library is needed to link.
# With libcurl's dev package installed, pkg-config supplies the flags.
# Without it there is no unversioned libcurl.so for plain -lcurl to find, so
# link the SONAME directly: libcurl.so.4 is what every distro's runtime
# package ships, whatever the exact version behind it.
# Consumers vendoring this repo link against the same way, so they copy this.
CURL_LIBS = $(shell pkg-config --libs libcurl 2>/dev/null || echo -l:libcurl.so.4)

all: $(LIB) $(BIN)

$(LIB): $(LIB_OBJS)
	ar rcs $@ $^

lib/%.o: lib/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN): $(CLI_OBJS) $(LIB)
	$(CC) -o $@ $(CLI_OBJS) $(LIB) $(CURL_LIBS)

cli/%.o: cli/%.c
	$(CC) -Wall -Wextra -O2 -Ilib -c -o $@ $<

clean:
	rm -f $(LIB_OBJS) $(CLI_OBJS) $(LIB) $(BIN)

install: $(BIN)
	install -m 755 $(BIN) /usr/local/bin/sscli

uninstall:
	rm -f /usr/local/bin/sscli

.PHONY: all clean install uninstall
