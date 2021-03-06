.PHONY: all clean test

all:
	$(CC) -Wall -Wextra -Wpedantic -D_GNU_SOURCE -nostartfiles -shared \
	 	-fpic -fPIC \
		-Wconversion -Wshadow \
		-Wpointer-arith -Wcast-qual \
		-Wstrict-prototypes -Wmissing-prototypes \
	 	-o libenospace.so libenospace.c -ldl \
	 	-Wl,-z,relro,-z,now -Wl,-z,noexecstack

clean:
	-@rm libenospace.so

test:
	@env LD_LIBRARY_PATH=. bats test
