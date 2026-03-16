# cupidwm build config

PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man
VERSION = 1.0
FONTDIR = ${PREFIX}/share/fonts/truetype/cupidwm

PKG_CONFIG ?= pkg-config
X11_PKGS = x11 xinerama xrandr xcursor xft fontconfig

ifeq ($(shell $(PKG_CONFIG) --exists $(X11_PKGS) >/dev/null 2>&1 && echo yes),yes)
INCS = $(shell $(PKG_CONFIG) --cflags $(X11_PKGS)) -I. -Isrc
LIBS = $(shell $(PKG_CONFIG) --libs $(X11_PKGS))
else
X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

INCS = -I${X11INC} -I/usr/include/freetype2 -I. -Isrc
LIBS = -L${X11LIB} -lX11 -lXinerama -lXrandr -lXcursor -lXft -lfontconfig
endif

CPPFLAGS = -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700L
WARN_CFLAGS = -Wall -Wextra -Wformat=2 -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wpointer-arith -Wcast-qual -Wwrite-strings -Wundef
WERROR ?= 1
ifeq (${WERROR},1)
WARN_CFLAGS += -Werror
endif

CFLAGS = -std=c99 -pedantic ${WARN_CFLAGS} -O2 ${CPPFLAGS} ${INCS} ${EXTRA_CFLAGS}
LDFLAGS = ${LIBS} ${EXTRA_LDFLAGS}

CC = cc
