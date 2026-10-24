CC ?= gcc
CFLAGS ?= -std=c11 -O3 -Wall -Wextra -Wpedantic -DNDEBUG
LDFLAGS ?=
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
TARGET ?= bigfiles

all: $(TARGET)

$(TARGET): bigfiles.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ bigfiles.c

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all install uninstall clean
