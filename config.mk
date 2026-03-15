# cupidwm build config

PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man
VERSION = 1.0

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

INCS = -I${X11INC} -I/usr/include/freetype2 -I. -Isrc
LIBS = -L${X11LIB} -lX11 -lXinerama -lXcursor -lXft -lfontconfig

CPPFLAGS = -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700L
CFLAGS = -std=c99 -pedantic -Wall -Wextra -O2 ${CPPFLAGS} ${INCS}
LDFLAGS = ${LIBS}

CC = cc
