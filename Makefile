CC=			gcc
CFLAGS=		-g -Wall -Wc++-compat -O2
CPPFLAGS=
INCLUDES=	-I.
PROG=		gfaview
LIBS=		-lz

.SUFFIXES:.c .o
.PHONY:all clean depend

.c.o:
		$(CC) -c $(CFLAGS) $(CPPFLAGS) $(INCLUDES) $< -o $@

all:$(PROG)

gfaview:gfa.o gfaview.o
		$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
		rm -fr gmon.out *.o a.out $(PROG) *~ *.a *.dSYM session*

depend:
		(LC_ALL=C; export LC_ALL; makedepend -Y -- $(CFLAGS) $(DFLAGS) -- *.c)

# DO NOT DELETE

gfa.o: kstring.h gfa.h kseq.h khash.h ksort.h kvec.h
gfaview.o: gfa.h
