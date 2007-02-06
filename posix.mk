GCCSDK_INSTALL_CROSSBIN ?= /home/riscos/cross/bin
GCCSDK_INSTALL_ENV ?= /home/riscos/env

CC = $(GCCSDK_INSTALL_CROSSBIN)/gcc
CC_DEBUG = /usr/bin/gcc
ASM = $(GCCSDK_INSTALL_CROSSBIN)/gcc

PLATFORM_CFLAGS_RISCOS = -I$(GCCSDK_INSTALL_ENV)/include \
		-I$(GCCSDK_INSTALL_ENV)/include/libxml2 \
		-I$(GCCSDK_INSTALL_ENV)/include/libmng \
		#-finstrument-functions
PLATFORM_CFLAGS_DEBUG = -I/usr/include/libxml2 -I/riscos/src/OSLib \
		-I/riscos/include/libjpeg -D_POSIX_C_SOURCE=200112
PLATFORM_AFLAGS_RISCOS = -I$(GCCSDK_INSTALL_ENV)/include

LDFLAGS_RISCOS = -L$(GCCSDK_INSTALL_ENV)/lib -lxml2 -lz -lcurl -lssl -lcrypto \
		-lcares -lmng -lOSLib32 -ljpeg -lrufl -lpencil #-lprof
LDFLAGS_SMALL = -L$(GCCSDK_INSTALL_ENV)/lib -lxml2 -lz -lucurl \
		-lcares -lmng -lOSLib32 -ljpeg -lrufl -lpencil
LDFLAGS_DEBUG = -L/usr/lib -lxml2 -lz -lm -lcurl -lssl -lcrypto -lmng \
		-ljpeg -llcms

# Hackery for Cygwin - it has no libdl, so remove it from LDFLAGS
ifeq ($(shell echo $$OS),Windows_NT)
LDFLAGS_DEBUG := $(subst -ldl,,$(LDFLAGS_DEBUG))
endif

RUNIMAGE = !NetSurf/!RunImage,ff8
NCRUNIMAGE = !NCNetSurf/!RunImage,ff8

# special cases - in here, cos RISC OS can't cope :(
css/css_enum.c css/css_enum.h: css/css_enums css/makeenum
	cd ..; perl netsurf/css/makeenum netsurf/css/css_enum < netsurf/css/css_enums
css/parser.c css/parser.h: css/parser.y
	-cd css; lemon parser.y
css/scanner.c: css/scanner.l
	cd css; re2c -s scanner.l > scanner.c
utils/translit.c: transtab
	cd utils; perl tt2code < transtab > translit.c

# remove generated files - again, RISC OS fails it
clean:
	-rm $(OBJDIR_RISCOS)/* $(OBJDIR_RISCOS_SMALL)/* $(OBJDIR_NCOS)/* \
		$(OBJDIR_DEBUG)/* $(OBJDIR_GTK)/* \
		css/css_enum.c css/css_enum.h \
		css/parser.c css/parser.h css/scanner.c \
		nsgtk
