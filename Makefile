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
	${RM} wendy
	${RM} -f *.o
	${RM} -f *~

path:
	@echo PREFIX: ${PREFIX}

install :
	install -D -m0755 wendy ${DESTDIR}${PREFIX}/bin/wendy

uninstall:
	${RM} ${DESTDIR}${PREFIX}/bin/wendy
