CC ?= gcc
CFLAGS ?= -std=c11 -O3 -Wall -Wextra -Wpedantic -DNDEBUG
LDFLAGS ?=
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
TOOLS ?= bigfiles changed

all: $(TOOLS)

bigfiles: bigfiles.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ bigfiles.c

changed: changed.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ changed.c

install: all
	install -d $(DESTDIR)$(BINDIR)
	for t in $(TOOLS); do install -m 0755 $$t $(DESTDIR)$(BINDIR)/$$t; done

uninstall:
	for t in $(TOOLS); do rm -f $(DESTDIR)$(BINDIR)/$$t; done

clean:
	rm -f $(TOOLS)

.PHONY: all install uninstall clean
