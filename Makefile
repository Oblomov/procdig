CFLAGS ?=
CFLAGS += -std=c99
CFLAGS += -O3
CFLAGS += -g
CFLAGS += -Wall -Wextra

LDFLAGS ?=
LDFLAGS += -lcrypto

basic:

clean:
	$(RM) -f basic
