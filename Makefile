# BEGINNING OF THE FILE

# Compilation settings
CC=gcc
CFLAGS=-Wall -I inc --std=c99 -pedantic
LDFLAGS=

# Command paths
RM=/bin/rm

.SUFFIXE :
.SUFFIXES : .c .o .h
.PHONY : all list mrproper clean init

.c.o:
	@echo -e "CC $<"
	@${CC} -c $< -o $@ ${CFLAGS}

wendy : wendy.o
	@echo -e "LD wendy"
	@${CC} $^ -o $@ ${LDFLAGS}

all : init wendy

clean :
	${RM} -f wendy *.o *~

install :
	install -D -m0755 wendy ${DESTDIR}${PREFIX}/bin/wendy

uninstall:
	${RM} ${DESTDIR}${PREFIX}/bin/wendy
## EOF
