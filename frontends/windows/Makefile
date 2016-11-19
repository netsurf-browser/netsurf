#
# Makefile for NetSurf Windows target
#
# This file is part of NetSurf 

LDFLAGS += -L${GCCSDK_INSTALL_ENV}/lib 
CFLAGS += -I${GCCSDK_INSTALL_ENV}/include/


$(eval $(call pkg_config_find_and_add,libcares,Cares))
$(eval $(call pkg_config_find_and_add,zlib,ZLib))

# libraries for windows API
LDFLAGS += -lgnurx -lgdi32 -lcomctl32 -lws2_32 -lmsimg32 -lshlwapi -mwindows

CFLAGS += -U__STRICT_ANSI__ -mwin32
# only windows versions after XP are supported
# https://msdn.microsoft.com/en-gb/library/windows/desktop/aa383745
CFLAGS += '-DWINVER=0x0501'
CFLAGS += '-D_WIN32_WINNT=0x0501'
CFLAGS += '-D_WIN32_WINDOWS=0x0501'
CFLAGS += '-D_WIN32_IE=0x0501'

#installed resource path
CFLAGS += '-DNETSURF_WINDOWS_RESPATH="$(NETSURF_WINDOWS_RESPATH)"'

WSCFLAGS := -std=c99 -Dnswin32 -DCURL_STATICLIB -DCARES_STATICLIB -g

CFLAGS += $(WSCFLAGS)
LDFLAGS += $(WSCFLAGS)

# ----------------------------------------------------------------------------
# built-in resource setup
# ----------------------------------------------------------------------------

$(OBJROOT)/windows_resource.o: $(FRONTEND_RESOURCES_DIR)/resource.rc
	$(VQ)echo " WINDRES: compiling windows resources"
	${Q}$(WINDRES) $< -O coff -o $@

S_RESOURCES := windows_resource.o

# ----------------------------------------------------------------------------
# Source file setup
# ----------------------------------------------------------------------------

# sources purely for the windows build
S_FRONTEND := main.c window.c gui.c drawable.c plot.c findfile.c	\
	  font.c bitmap.c about.c prefs.c download.c filetype.c file.c	\
	  localhistory.c schedule.c windbg.c pointers.c \
	  corewindow.c hotlist.c cookies.c global_history.c ssl_cert.c

# This is the final source build list
# Note this is deliberately *not* expanded here as common and image
#   are not yet available
SOURCES = $(S_COMMON) $(S_IMAGE) $(S_BROWSER) $(S_FRONTEND) $(S_RESOURCES)
EXETARGET := NetSurf.exe

# ----------------------------------------------------------------------------
# Install target
# ----------------------------------------------------------------------------

install-windows:

# ----------------------------------------------------------------------------
# Package target
# ----------------------------------------------------------------------------

package-windows: netsurf-installer.exe

WIN_RES_OBJ := installer.nsi NetSurf.ico netsurf.png welcome.html default.css
WIN_RES_INS_OBJ := $(addprefix $(FRONTEND_RESOURCES_DIR)/,$(WIN_RES_OBJ)) $(OBJROOT)/messages

$(OBJROOT)/messages: resources/FatMessages
	$(Q)$(SPLIT_MESSAGES) -l en -p win -f messages resources/FatMessages > $@

netsurf-installer.exe: $(EXETARGET) $(WIN_RES_INS_OBJ)
	makensis -V4 -NOCD $(FRONTEND_RESOURCES_DIR)/installer.nsi 
