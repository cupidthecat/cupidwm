# cupidwm - compact X11 window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = src/cupidwm.c
OBJ = build/cupidwm.o

all: cupidwm

build:
	mkdir -p build

build/cupidwm.o: src/cupidwm.c config.h config.mk src/defs.h | build
	${CC} -c ${CFLAGS} src/cupidwm.c -o build/cupidwm.o

config.h:
	cp config.def.h $@

cupidwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -rf build cupidwm cupidwm.o cupidwm-${VERSION}.tar.gz

dist: clean
	mkdir -p cupidwm-${VERSION}
	cp -R LICENSE Makefile README.md config.def.h config.mk cupidwm.1 src cupidwm-${VERSION}
	tar -cf cupidwm-${VERSION}.tar cupidwm-${VERSION}
	gzip cupidwm-${VERSION}.tar
	rm -rf cupidwm-${VERSION}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f cupidwm ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/cupidwm
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/1.0/g" < cupidwm.1 > ${DESTDIR}${MANPREFIX}/man1/cupidwm.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/cupidwm.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/cupidwm \
		${DESTDIR}${MANPREFIX}/man1/cupidwm.1

.PHONY: all clean dist install uninstall
