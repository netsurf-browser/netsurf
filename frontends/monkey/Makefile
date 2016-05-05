#
# Makefile for NetSurf monkey target
#
# This file is part of NetSurf 

# ----------------------------------------------------------------------------
# Monkey flag setup (using pkg-config)
# ----------------------------------------------------------------------------

CWARNFLAGS += -Werror

CFLAGS += -std=c99 -Dmonkey -Dnsmonkey -g \
	  -D_BSD_SOURCE \
	  -D_DEFAULT_SOURCE \
	  -D_XOPEN_SOURCE=700 \
	  -D_POSIX_C_SOURCE=200809L \
	  -D_NETBSD_SOURCE \
	  -DMONKEY_RESPATH=\"$(NETSURF_MONKEY_RESOURCES)\"

LDFLAGS += -lm

# ---------------------------------------------------------------------------
# Windows flag setup
# ---------------------------------------------------------------------------

ifeq ($(HOST),Windows_NT)
  CFLAGS += -U__STRICT_ANSI__
endif

# ----------------------------------------------------------------------------
# Source file setup
# ----------------------------------------------------------------------------

# S_MONKEY are sources purely for the MONKEY build
S_FRONTEND := main.c filetype.c schedule.c bitmap.c plot.c browser.c \
	download.c 401login.c cert.c layout.c dispatch.c fetch.c


# This is the final source build list
# Note this is deliberately *not* expanded here as common and image
#   are not yet available
SOURCES = $(S_COMMON) $(S_IMAGE) $(S_BROWSER) $(S_PDF) $(S_FRONTEND)
EXETARGET := nsmonkey

# ----------------------------------------------------------------------------
# Install target
# ----------------------------------------------------------------------------

install-monkey:

# ----------------------------------------------------------------------------
# Package target
# ----------------------------------------------------------------------------

package-monkey:
