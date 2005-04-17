CC = /home/riscos/cross/bin/gcc
CC_DEBUG = gcc

PLATFORM_CFLAGS_RISCOS = -I/home/riscos/env/include \
		-I/home/riscos/env/include/libxml2 \
		-I/home/riscos/env/include/libmng
PLATFORM_CFLAGS_DEBUG = -I/usr/include/libxml2 -I/riscos/src/OSLib \
		-I/riscos/include/libjpeg -D_POSIX_C_SOURCE

LDFLAGS_RISCOS = -L/home/riscos/env/lib -lxml2 -lz -lcurl -lssl -lcrypto \
		-lcares -lmng -loslib -ljpeg -lrufl
LDFLAGS_SMALL = -L/home/riscos/env/lib -lxml2 -lz -lucurl -lcares -lmng \
		-loslib -ljpeg -lrufl
LDFLAGS_DEBUG = -L/usr/lib -lxml2 -lz -lm -lcurl -lssl -lcrypto -ldl -lmng \
		-ljpeg -llcms

RUNIMAGE = !NetSurf/!RunImage,ff8
NCRUNIMAGE = !NCNetSurf/!RunImage,ff8
