CC = gcc
CFLAGS = -Wall -Wextra -O2 -fPIC -Ivendor/include -Ilib

LIB_SRCS = lib/ss_api.c lib/ss_json.c lib/ss_config.c lib/ss_state.c lib/ss_utils.c
LIB_OBJS = $(LIB_SRCS:.c=.o)

CLI_SRCS = cli/main.c cli/output.c
CLI_OBJS = $(CLI_SRCS:.c=.o)

LIB = lib/libss.so
BIN = simple-social-cli

all: vendor-links $(LIB) $(BIN)

vendor-links:
	@mkdir -p vendor
	@if [ ! -e vendor/libcurl.so ]; then ln -sf /usr/lib/x86_64-linux-gnu/libcurl.so.4.8.0 vendor/libcurl.so; echo "linked vendor/libcurl.so"; fi

$(LIB): $(LIB_OBJS)
	$(CC) -shared -o $@ $^ -Lvendor -lcurl

lib/%.o: lib/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BIN): $(CLI_OBJS) $(LIB)
	$(CC) -o $@ $(CLI_OBJS) -Llib -lss -Wl,-rpath,'$$ORIGIN/../lib' -Wl,-rpath,'$$ORIGIN/lib'

cli/%.o: cli/%.c
	$(CC) -Wall -Wextra -O2 -Ilib -c -o $@ $<

clean:
	rm -f $(LIB_OBJS) $(CLI_OBJS) $(LIB) $(BIN)

install: $(BIN)
	install -m 755 $(BIN) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(BIN)

.PHONY: all clean install uninstall vendor-links
