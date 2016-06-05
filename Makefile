CFLAGS ?=
CFLAGS += -std=c99
CFLAGS += -O3
CFLAGS += -g
CFLAGS += -Wall -Wextra

LDFLAGS ?=
LDFLAGS += -lcrypto

PROGS=basic sha256rng

all: $(PROGS)

clean:
	$(RM) -f $(PROGS)
