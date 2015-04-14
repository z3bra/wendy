include config.mk

.SUFFIXES : .c .o
.PHONY : all list clean install uninstall

.c.o:
	@echo -e "CC $<"
	@${CC} -c $< -o $@ ${CFLAGS}

wendy : wendy.o
	@echo -e "LD wendy"
	@${CC} $^ -o $@ ${LDFLAGS}

all : wendy

clean :
	${RM} -f wendy *.o *~

path:
	@echo PREFIX: ${PREFIX}

install :
	install -D -m0755 wendy ${DESTDIR}${PREFIX}/bin/wendy
	install -D -m0644 wendy.1 ${DESTDIR}${MANPREFIX}/man1/wendy.1

uninstall:
	${RM} ${DESTDIR}${PREFIX}/bin/wendy
	${RM} ${DESTDIR}${MANPREFIX}/man1/wendy.1
