CC = gcc
CC_DEBUG = gcc

PLATFORM_CFLAGS_RISCOS = -mthrowback -INSLibs:include -IOSLib:
PLATFORM_CFLAGS_DEBUG = -mthrowback -INSLibs:include -IOSLib:

LDFLAGS_RISCOS = NSLibs:lib/libxml2 NSLibs:lib/libz NSLibs:lib/libcurl \
	NSLibs:lib/libssl NSLibs:lib/libcrypto NSLibs:lib/libcares \
	NSLibs:lib/libmng NSLibs:lib/libjpeg OSLib:o.oslib32
LDFLAGS_SMALL = NSLibs:lib/libxml2 NSLibs:lib/libz NSLibs:lib/libcurl \
	NSLibs:lib/libares NSLibs:lib/libmng \
	NSLibs:lib/libjpeg OSLib:o.oslib32

RUNIMAGE = !NetSurf/!RunImage
NCRUNIMAGE = !NCNetSurf/!RunImage
