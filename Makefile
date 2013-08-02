# BEGINNING OF THE FILE

# Compilation settings
PROG=wendy
CC=gcc
CFLAGS=-Wall -I inc --std=c99 -pedantic
LDFLAGS=

# Command paths
RM=/bin/rm

.PHONY : all list mrproper clean init

$(PROG) : $(PROG).o
	@echo -e "LD $(PROG)"
	@$(CC) $^ -o$@ $(LDFLAGS)

$(PROG).o : $(PROG).c
	@echo -e "CC $<"
	@$(CC) -c $(CFLAGS) $< -o $@

all : clean $(PROG)

mrproper : clean
	$(RM) $(PROG)

clean :
	$(RM) -f *.o
	$(RM) -f *~

init :
	@echo "CC = $(CC)"
	@echo "CFLAGS = $(CFLAGS)"
	@echo "LDFLAGS = $(LDFLAGS)"
	@echo
## EOF
