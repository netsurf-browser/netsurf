CC = /riscos/bin/gcc
CC_DEBUG = gcc

PLATFORM_CFLAGS_RISCOS =
PLATFORM_CFLAGS_DEBUG = -I/usr/include/libxml2 -I/riscos/src/OSLib \
		-I/riscos/include/libjpeg -D_POSIX_C_SOURCE

LDFLAGS_RISCOS = -L/riscos/lib -lxml2 -lz -lcurl -lssl -lcrypto -lcares -lpng \
		-loslib -ljpeg
LDFLAGS_SMALL = -L/riscos/lib -lxml2 -lz -lucurl -lcares -lpng -loslib -ljpeg
LDFLAGS_DEBUG = -L/usr/lib -lxml2 -lz -lm -lcurl -lssl -lcrypto -ldl -lpng \
		-ljpeg

RUNIMAGE = !NetSurf/!RunImage,ff8
