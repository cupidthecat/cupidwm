# cupidwm - compact X11 window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = src/cupidwm.c
OBJ = build/cupidwm.o
CTL_SRC = tools/cupidwmctl.c
CTL_OBJ = build/cupidwmctl.o

SAN_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer

all: release

release: clean cupidwm cupidwmctl

debug: EXTRA_CFLAGS += -O0 -g3 ${SAN_FLAGS}
debug: EXTRA_LDFLAGS += ${SAN_FLAGS}
debug: clean cupidwm

build:
	mkdir -p build

build/cupidwm.o: src/cupidwm.c config.h config.mk src/defs.h | build
	${CC} -c ${CFLAGS} src/cupidwm.c -o build/cupidwm.o

config.h:
	cp config.def.h $@

cupidwm: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

cupidwmctl: ${CTL_OBJ}
	${CC} -o $@ ${CTL_OBJ} ${EXTRA_LDFLAGS}

build/cupidwmctl.o: ${CTL_SRC} | build
	${CC} -c -std=c99 -pedantic -Wall -Wextra -O2 ${CTL_SRC} -o build/cupidwmctl.o

preflight-test-scripts:
	@test -f ./scripts/smoke-xephyr.sh || { echo "missing required script: ./scripts/smoke-xephyr.sh"; exit 1; }
	@test -f ./scripts/install-deps.sh || { echo "missing required script: ./scripts/install-deps.sh"; exit 1; }
	@test -f ./tests/ewmh/invariants.sh || { echo "missing required script: ./tests/ewmh/invariants.sh"; exit 1; }
	@test -f ./tests/ewmh/send-client-message.c || { echo "missing required file: ./tests/ewmh/send-client-message.c"; exit 1; }

test-smoke: preflight-test-scripts cupidwm
	bash ./scripts/smoke-xephyr.sh ./cupidwm

test-ewmh: preflight-test-scripts cupidwm
	bash ./tests/ewmh/invariants.sh ./cupidwm

test: test-smoke test-ewmh

lint:
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=warning,performance,portability --error-exitcode=1 --quiet src; \
	else \
		echo "cppcheck not found; skipping static analysis"; \
	fi
	@if command -v shellcheck >/dev/null 2>&1; then \
		files=$$(find scripts tests -type f -name '*.sh'); \
		if [ -n "$$files" ]; then \
			shellcheck -x $$files; \
		else \
			echo "no shell scripts found for shellcheck"; \
		fi; \
	else \
		echo "shellcheck not found; skipping shell script analysis"; \
	fi

check: debug lint
	@if [ -n "$$DISPLAY" ] && \
		command -v Xephyr >/dev/null 2>&1 && \
		command -v xdpyinfo >/dev/null 2>&1 && \
		command -v xprop >/dev/null 2>&1 && \
		command -v xdotool >/dev/null 2>&1 && \
		command -v xterm >/dev/null 2>&1 && \
		command -v xwininfo >/dev/null 2>&1; then \
		${MAKE} --no-print-directory test-smoke && \
		${MAKE} --no-print-directory test-ewmh; \
	elif [ -z "$$DISPLAY" ] && \
		command -v xvfb-run >/dev/null 2>&1 && \
		command -v Xephyr >/dev/null 2>&1 && \
		command -v xdpyinfo >/dev/null 2>&1 && \
		command -v xprop >/dev/null 2>&1 && \
		command -v xdotool >/dev/null 2>&1 && \
		command -v xterm >/dev/null 2>&1 && \
		command -v xwininfo >/dev/null 2>&1; then \
		echo "DISPLAY not set; running integration suites under xvfb-run"; \
		xvfb-run -a ${MAKE} --no-print-directory test-smoke && \
		xvfb-run -a ${MAKE} --no-print-directory test-ewmh; \
	else \
		echo "Smoke test skipped (missing DISPLAY or Xephyr/x11 test dependencies)"; \
	fi

clean:
	rm -rf build cupidwm cupidwmctl cupidwm.o cupidwm-${VERSION}.tar.gz

dist: clean
	mkdir -p cupidwm-${VERSION}
	cp -R LICENSE LICENSES THIRD_PARTY_NOTICE.md SECURITY.md CHANGELOG.md \
		Makefile README.md config.def.h config.mk cupidwm.1 cupidwm.desktop \
		src scripts tests tools docs img undefined-medium.ttf cupidwm-${VERSION}
	@if [ -d .github ]; then cp -R .github cupidwm-${VERSION}; fi
	tar -cf cupidwm-${VERSION}.tar cupidwm-${VERSION}
	gzip cupidwm-${VERSION}.tar
	rm -rf cupidwm-${VERSION}

distcheck: dist
	@tmpdir=$$(mktemp -d); \
	tar -xf cupidwm-${VERSION}.tar.gz -C "$$tmpdir"; \
	test -f "$$tmpdir/cupidwm-${VERSION}/Makefile"; \
	test -f "$$tmpdir/cupidwm-${VERSION}/README.md"; \
	test -f "$$tmpdir/cupidwm-${VERSION}/SECURITY.md"; \
	test -f "$$tmpdir/cupidwm-${VERSION}/THIRD_PARTY_NOTICE.md"; \
	test -f "$$tmpdir/cupidwm-${VERSION}/docs/configuration.md"; \
	test -f "$$tmpdir/cupidwm-${VERSION}/src/cupidwm.c"; \
	test -f "$$tmpdir/cupidwm-${VERSION}/scripts/smoke-xephyr.sh"; \
	test -f "$$tmpdir/cupidwm-${VERSION}/tests/ewmh/invariants.sh"; \
	test -f "$$tmpdir/cupidwm-${VERSION}/tools/cupidwmctl.c"; \
	rm -rf "$$tmpdir"; \
	echo "distcheck passed"

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f cupidwm ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/cupidwm
	cp -f cupidwmctl ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/cupidwmctl
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < cupidwm.1 > ${DESTDIR}${MANPREFIX}/man1/cupidwm.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/cupidwm.1
	mkdir -p ${DESTDIR}${PREFIX}/share/xsessions
	cp -f cupidwm.desktop ${DESTDIR}${PREFIX}/share/xsessions/cupidwm.desktop
	chmod 644 ${DESTDIR}${PREFIX}/share/xsessions/cupidwm.desktop

install-font:
	mkdir -p ${DESTDIR}${FONTDIR}
	cp -f undefined-medium.ttf ${DESTDIR}${FONTDIR}/undefined-medium.ttf
	chmod 644 ${DESTDIR}${FONTDIR}/undefined-medium.ttf
	if command -v fc-cache >/dev/null 2>&1; then fc-cache -f ${DESTDIR}${FONTDIR} || true; fi

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/cupidwm \
		${DESTDIR}${PREFIX}/bin/cupidwmctl \
		${DESTDIR}${MANPREFIX}/man1/cupidwm.1 \
		${DESTDIR}${PREFIX}/share/xsessions/cupidwm.desktop

uninstall-font:
	rm -f ${DESTDIR}${FONTDIR}/undefined-medium.ttf

.PHONY: all release debug clean lint check dist distcheck install install-font uninstall uninstall-font test test-smoke test-ewmh preflight-test-scripts
