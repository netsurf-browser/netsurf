CC = gcc
CC_DEBUG = gcc

PLATFORM_CFLAGS_RISCOS = -INSLibs:include -IOSLib:
PLATFORM_CFLAGS_DEBUG = -INSLibs:include -IOSLib:

LDFLAGS_RISCOS = NSLibs:lib/libxml2 NSLibs:lib/libz NSLibs:lib/libcurl \
	NSLibs:lib/libssl NSLibs:lib/libcrypto NSLibs:lib/libares \
	NSLibs:lib/libpng NSLibs:lib/libjpeg OSLib:o.oslib32
LDFLAGS_SMALL = NSLibs:lib/libxml2 NSLibs:lib/libz NSLibs:lib/libcurl \
	NSLibs:lib/libares NSLibs:lib/libpng \
	NSLibs:lib/libjpeg OSLib:o.oslib32

RUNIMAGE = !NetSurf/!RunImage
NCRUNIMAGE = !NCNetSurf/!RunImage
