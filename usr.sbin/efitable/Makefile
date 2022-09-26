#	$OpenBSD$
#
PROG=	efitable
MAN=	efitable.8
CFLAGS=	-I/sys
#
#.include <bsd.prog.mk>

efitable: efitable.o
	$(CC) -o efitable efitable.o

clean:
	rm -f *.o

