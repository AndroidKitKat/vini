LIB=		vinny
LIBDIR=		../../../mods
SHLIB_MAJOR=	1
SHLIB_MINOR=	0

SRCS=		cmds.c handlers.c hooks.c loader.c logs.c mods.c ops.c	\
		parser.c regex.c vinny.c watcher.c
CFLAGS+=	-I../../ -I/usr/local/include
LDFLAGS+=	-L/usr/local/lib -L../../irc -L../../info -lpcre -lpq -lpthread -lirc -lircinfo

.include <bsd.lib.mk>
