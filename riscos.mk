CC = gcc
CC_DEBUG = gcc
ASM = gcc

PLATFORM_CFLAGS_RISCOS = -mthrowback -INSLibs:include -IOSLib:
PLATFORM_CFLAGS_DEBUG = -mthrowback -INSLibs:include -IOSLib:
PLATFORM_AFLAGS_RISCOS = -mthrowback -IOSLib:

LDFLAGS_RISCOS = NSLibs:lib/libxml2 NSLibs:lib/libz NSLibs:lib/libcurl \
	NSLibs:lib/libssl NSLibs:lib/libcrypto NSLibs:lib/libcares \
	NSLibs:lib/libmng NSLibs:lib/libjpeg NSLibs:lib/librufl NSLibs:lib/libpencil \
	OSLib:o.OSLib32
LDFLAGS_SMALL = NSLibs:lib/libxml2 NSLibs:lib/libz NSLibs:lib/libcurl \
	NSLibs:lib/libares NSLibs:lib/libmng \
	NSLibs:lib/libjpeg OSLib:o.oslib32

RUNIMAGE = !NetSurf/!RunImage
NCRUNIMAGE = !NCNetSurf/!RunImage

# special cases
css/css_enum.c css/css_enum.h: css/css_enums css/makeenum
	@dir ^
	perl netsurf/css/makeenum netsurf/css/css_enum < netsurf/css/css_enums
	@dir netsurf
css/parser.c css/parser.h: css/parser.y
	@dir css
	-lemon parser.y
	@dir ^
css/scanner.c: css/scanner.l
	@dir css
	re2c -s scanner.l > scanner.c
	@dir ^
utils/translit.c: transtab
	@dir utils
	perl tt2code < transtab > translit.c
	@dir ^

# remove generated files
clean:
	-wipe $(OBJDIR_RISCOS).* ~CFR~V
	-wipe $(OBJDIR_RISCOS_SMALL).* ~CFR~V
	-wipe $(OBJDIR_NCOS).* ~CFR~V
	-wipe $(OBJDIR_DEBUG).* ~CFR~V
	-wipe $(OBJDIR_GTK).* ~CFR~V
	-wipe css.c.css_enum ~CFR~V
	-wipe css.h.css_enum ~CFR~V
	-wipe css.c.parser ~CFR~V
	-wipe css.h.parser ~CFR~V
	-wipe css.c.scanner ~CFR~V
