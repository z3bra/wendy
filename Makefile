include config.mk

.PHONY: clean install uninstall

wendy: wendy.o

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

.o:
	$(LD) $(LDFLAGS) $< -o $@

clean :
	rm -f wendy *.o

install :
	install -D -m0755 wendy ${DESTDIR}${PREFIX}/bin/wendy
	install -D -m0644 wendy.1 ${DESTDIR}${MANPREFIX}/man1/wendy.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/wendy
	rm -f ${DESTDIR}${MANPREFIX}/man1/wendy.1
