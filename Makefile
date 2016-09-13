CC=			gcc
CFLAGS=		-g -Wall -Wc++-compat -O2
CPPFLAGS=
INCLUDES=	-I.
OBJS=		gfa.o
PROG=		gfa2
LIBS=		-lz

.SUFFIXES:.c .o
.PHONY:all demo clean depend

.c.o:
		$(CC) -c $(CFLAGS) $(CPPFLAGS) $(INCLUDES) $< -o $@

all:$(PROG)

gfa2:$(OBJS) main.o fmt.o
		$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
		rm -fr gmon.out *.o a.out $(PROG) *~ *.a *.dSYM session*

depend:
		(LC_ALL=C; export LC_ALL; makedepend -Y -- $(CFLAGS) $(DFLAGS) -- *.c)

# DO NOT DELETE

fmt.o: gfa.h
gfa.o: kstring.h gfa.h kseq.h khash.h ksort.h
main.o: gfa.h
