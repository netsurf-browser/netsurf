CC = /riscos/bin/gcc
CC_DEBUG = gcc

PLATFORM_CFLAGS = 
PLATFORM_CFLAGS_DEBUG = -I/usr/include/libxml2 -I/riscos/include

LDFLAGS = -L/riscos/lib -lxml2 -lz -lcurl -lssl -lcrypto -lcares -lanim -lpng \
	-loslib -ljpeg
LDFLAGS_SMALL = -L/riscos/lib -lxml2 -lz -lucurl -lcares -lanim -lpng -loslib -ljpeg
LDFLAGS_DEBUG = -L/usr/lib -lxml2 -lz -lm -lcurl -lssl -lcrypto -ldl -ljpeg

!NetSurf/!RunImage,ff8 : $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^

include depend
