CC=			gcc
CFLAGS=		-g -Wall -Wc++-compat -O2
CPPFLAGS=
INCLUDES=	-I..
PROG=		fastg2gfa falcon2gfa mag2gfa
LIBS=		-lz

.SUFFIXES:.c .o
.PHONY:all clean depend

.c.o:
		$(CC) -c $(CFLAGS) $(CPPFLAGS) $(INCLUDES) $< -o $@

all:$(PROG)

fastg2gfa:fastg2gfa.o
		$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

falcon2gfa:falcon2gfa.o
		$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

mag2gfa:mag2gfa.o
		$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

clean:
		rm -fr gmon.out *.o a.out $(PROG) *~ *.a *.dSYM session*

depend:
		(LC_ALL=C; export LC_ALL; makedepend -Y -- $(CFLAGS) $(DFLAGS) -- *.c)

# DO NOT DELETE
