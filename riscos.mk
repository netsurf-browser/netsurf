CC = gcc
CC_DEBUG = gcc

PLATFORM_CFLAGS = -INSLibs:include -IOSLib:
PLATFORM_CFLAGS_DEBUG = -INSLibs:include -IOSLib:

LDFLAGS = NSLibs:lib/libxml2 NSLibs:lib/libz NSLibs:lib/libcurl \
	NSLibs:lib/libssl NSLibs:lib/libcrypto NSLibs:lib/libares \
	NSLibs:lib/libanim NSLibs:lib/libpng NSLibs:lib/libjpeg OSLib:o.oslib32
LDFLAGS_SMALL = NSLibs:lib/libxml2 NSLibs:lib/libz NSLibs:lib/libcurl \
	NSLibs:lib/libares NSLibs:lib/libanim NSLibs:lib/libpng \
	NSLibs:lib/libjpeg OSLib:o.oslib32
LDFLAGS_DEBUG = NSLibs:lib/libxml2 NSLibs:lib/libz NSLibs:lib/libcurl \
	NSLibs:lib/libssl NSLibs:lib/libcrypto NSLibs:lib/libares \
	NSLibs:lib/libanim NSLibs:lib/libpng NSLibs:lib/libjpeg OSLib:o.oslib32

!NetSurf/!RunImage,ff8 : $(OBJS)
	$(CC) -o !NetSurf/!RunImage $(LDFLAGS) $^
