LDFLAGS+=-lX11 -lXrandr
CCFLAGS=-c99 -Wall -Wextra -pedantic -Werror

PREFIX=/usr

all: onrandr
clean:
	rm -f onrandr

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin/
	cp onrandr $(DESTDIR)$(PREFIX)/bin/
