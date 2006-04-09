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
		-I/riscos/include/libjpeg -D_POSIX_C_SOURCE
PLATFORM_AFLAGS_RISCOS = -I$(GCCSDK_INSTALL_ENV)/include

LDFLAGS_RISCOS = -L$(GCCSDK_INSTALL_ENV)/lib -lxml2 -lz -lcurl -lssl -lcrypto \
		-lcares -lmng -lOSLib32 -ljpeg -lrufl -lpencil #-lprof
LDFLAGS_SMALL = -L$(GCCSDK_INSTALL_ENV)/lib -lxml2 -lz -lucurl \
		-lcares -lmng -lOSLib32 -ljpeg -lrufl -lpencil
LDFLAGS_DEBUG = -L/usr/lib -lxml2 -lz -lm -lcurl -lssl -lcrypto -ldl -lmng \
		-ljpeg -llcms

# Hackery for Cygwin - it has no libdl, so remove it from LDFLAGS
ifeq ($(shell echo $$OS),Windows_NT)
LDFLAGS_DEBUG := $(subst -ldl,,$(LDFLAGS_DEBUG))
endif

RUNIMAGE = !NetSurf/!RunImage,ff8
NCRUNIMAGE = !NCNetSurf/!RunImage,ff8
