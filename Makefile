include config.mk

.PHONY: clean install uninstall

wendy: wendy.o

install: wendy wendy.1
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	cp wendy $(DESTDIR)$(PREFIX)/bin/wendy
	cp wendy.1 $(DESTDIR)$(MANPREFIX)/man1/wendy.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/wendy
	rm -f $(DESTDIR)$(MANPREFIX)/man1/wendy.1

clean:
	rm -f wendy *.o
