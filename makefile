.POSIX:

BINARY = dec
PREFIX = /usr/local

all:
	CGO_ENABLED=0 go build -o $(BINARY)

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(BINARY) $(DESTDIR)$(PREFIX)/bin/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BINARY)

clean:
	rm -f $(BINARY)

.PHONY: all install uninstall clean
