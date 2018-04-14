CFLAGS ?=
CFLAGS += -std=c99
CFLAGS += -O3
CFLAGS += -g
CFLAGS += -Wall -Wextra

LDFLAGS ?=
LDFLAGS += -lcrypto -lm

PROGS=basic sha256rng svg-magic-circle

all: $(PROGS)

clean:
	$(RM) -f $(PROGS)
