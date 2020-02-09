LDFLAGS+=-lX11 -lXrandr
CFLAGS+=-std=c99 -Wall -Wextra -pedantic -Werror
CFLAGS+=-Os -ffunction-sections -fdata-sections -Wl,--gc-sections
CFLAGS+=-D_DEFAULT_SOURCE

PREFIX=/usr

all: onrandr
clean:
	rm -f onrandr

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin/
	cp onrandr $(DESTDIR)$(PREFIX)/bin/
