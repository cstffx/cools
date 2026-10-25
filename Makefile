CC ?= gcc
CFLAGS ?= -std=c11 -O3 -Wall -Wextra -Wpedantic -DNDEBUG
LDFLAGS ?=
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
MANDIR ?= $(PREFIX)/share/man/man1
COMPLETIONDIR ?= $(PREFIX)/share/bash-completion/completions
TOOLS ?= bigfiles changed fileinfo contextpack
MANPAGES ?= man/bigfiles.1 man/changed.1 man/fileinfo.1 man/contextpack.1
COMPLETIONS ?= completions/bigfiles completions/changed completions/fileinfo completions/contextpack

all: $(TOOLS)

bigfiles: bigfiles.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ bigfiles.c -lmagic

changed: changed.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ changed.c

fileinfo: fileinfo.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ fileinfo.c -lmagic

contextpack: contextpack.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ contextpack.c -lmagic

install: all
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR) $(DESTDIR)$(COMPLETIONDIR)
	for t in $(TOOLS); do install -m 0755 $$t $(DESTDIR)$(BINDIR)/$$t; done
	for m in $(MANPAGES); do install -m 0644 $$m $(DESTDIR)$(MANDIR)/; done
	for c in $(COMPLETIONS); do install -m 0644 $$c $(DESTDIR)$(COMPLETIONDIR)/$$(basename $$c); done

uninstall:
	for t in $(TOOLS); do rm -f $(DESTDIR)$(BINDIR)/$$t; done

clean:
	rm -f $(TOOLS) *.deb
	rm -rf cools_*_*.deb.tmpdir

VERSION ?= 1.0.0
ARCH ?= $(shell dpkg --print-architecture)
DEB := cools_$(VERSION)_$(ARCH).deb
PKGDIR := cools_$(VERSION)_$(ARCH).deb.tmpdir

deb: all
	rm -rf $(PKGDIR)
	mkdir -p $(PKGDIR)/DEBIAN $(PKGDIR)/usr/bin $(PKGDIR)/usr/share/man/man1 $(PKGDIR)/usr/share/bash-completion/completions
	cp $(TOOLS) $(PKGDIR)/usr/bin/
	cp $(MANPAGES) $(PKGDIR)/usr/share/man/man1/
	cp $(COMPLETIONS) $(PKGDIR)/usr/share/bash-completion/completions/
	printf 'Package: cools\nVersion: %s\nSection: utils\nPriority: optional\nArchitecture: %s\nMaintainer: cstffx <cstffx@gmail.com>\nDepends: libc6, libmagic1\nDescription: Command-line tools to help AI agents work with the file system.\n Includes bigfiles, changed, fileinfo and contextpack.\n' '$(VERSION)' '$(ARCH)' > $(PKGDIR)/DEBIAN/control
	dpkg-deb --root-owner-group --build $(PKGDIR) $(DEB)
	rm -rf $(PKGDIR)

.PHONY: all install uninstall clean
