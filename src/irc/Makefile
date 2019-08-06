LIB=		irc
LIBDIR=		/usr/local/lib
SHLIB_MAJOR=	1
SHLIB_MINOR=	0

CFLAGS+=	-I../
LDFLAGS+=	-lssl -lpthread
SRCS=		irc.c hooks.c ircparser.c utils.c core.c

.include <bsd.lib.mk>
