LIB=		louis
LIBDIR=		../../../mods
SHLIB_MAJOR=	1
SHLIB_MINOR=	0

SRCS=		louis.c periodic.c lualib.c
CFLAGS+=	-I../../ -I/usr/local/include/lua52
LDFLAGS+=	-L/usr/local/lib -L../../irc -L../../info -llua-5.2 -lpthread -lirc -lircinfo

.include <bsd.lib.mk>
