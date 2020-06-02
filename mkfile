<config.mk

wendy: wendy.o
	${LD} $prereq ${LDFLAGS} ${LIBS} -o $target

wendy.o: wendy.c
	${CC} -c -o $target $prereq ${CPPFLAGS} ${CFLAGS}

clean:V:
	rm -f wendy *.o

install:V: wendy wendy.1
	mkdir -p ${DESTDIR}${PREFIX}/bin
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	cp wendy ${DESTDIR}${PREFIX}/bin/wendy
	cp wendy.1 ${DESTDIR}${MANPREFIX}/man1/
	chmod 755 ${DESTDIR}${PREFIX}/bin/wendy

uninstall:V:
	rm ${DESTDIR}${PREFIX}/bin/wendy
	rm ${DESTDIR}${MANPREFIX}/man1/wendy.1
