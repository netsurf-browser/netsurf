#
# This file is part of NetSurf, http://netsurf-browser.org/
# Licensed under the GNU General Public License,
#                http://www.opensource.org/licenses/gpl-license
#

# There are 6 possible builds of NetSurf:
#
#   riscos -- standard RISC OS build
#   riscos_small -- identical to "riscos", but linked with smaller libraries
#   		(no openssl, and libcurl without ssl support)
#   debug -- command line Unix/Linux, for debugging
#   riscos_debug -- a cross between "riscos" and "debug"
#   gtk -- experimental gtk version
#
# "riscos", "riscos_small", and "riscos_debug" can be compiled under
# RISC OS, or cross-compiled using GCCSDK.

SYSTEM_CC ?= gcc

OBJECTS_COMMON = content.o fetch.o fetchcache.o urldb.o		# content/
OBJECTS_COMMON += fetch_curl.o                                  # content/fetchers/
OBJECTS_COMMON += css.o css_enum.o parser.o ruleset.o scanner.o	# css/
OBJECTS_COMMON += box.o box_construct.o box_normalise.o directory.o \
	form.o html.o html_redraw.o imagemap.o layout.o list.o \
	table.o textplain.o					# render/
OBJECTS_COMMON += filename.o hashtable.o messages.o talloc.o \
	url.o utf8.o utils.o useragent.o			# utils/
OBJECTS_COMMON += knockout.o options.o tree.o version.o		# desktop/

OBJECTS_IMAGE = bmp.o bmpread.o gif.o gifread.o ico.o jpeg.o \
	mng.o svg.o rsvg.o					# image/

OBJECTS_RISCOS = $(OBJECTS_COMMON) $(OBJECTS_IMAGE)
OBJECTS_RISCOS += browser.o frames.o history_core.o netsurf.o \
	selection.o textinput.o 				# desktop/
OBJECTS_RISCOS += 401login.o artworks.o assert.o awrender.o bitmap.o \
	buffer.o cookies.o configure.o debugwin.o \
	dialog.o download.o draw.o filetype.o font.o \
	global_history.o gui.o help.o history.o hotlist.o image.o \
	menus.o message.o palettes.o plotters.o plugin.o print.o \
	query.o save.o save_complete.o save_draw.o save_text.o \
	schedule.o search.o sprite.o sslcert.o textarea.o \
	textselection.o theme.o theme_install.o thumbnail.o \
	treeview.o ucstables.o uri.o url_complete.o url_protocol.o \
	wimp.o wimp_event.o window.o				# riscos/
OBJECTS_RISCOS += con_cache.o con_connect.o con_content.o con_fonts.o \
	con_home.o con_image.o con_inter.o con_language.o con_memory.o \
	con_secure.o con_theme.o		 		# riscos/configure/
OBJECTS_RISCOS += progress_bar.o status_bar.o	 		# riscos/gui/
# OBJECTS_RISCOS += memdebug.o

OBJECTS_RISCOS_SMALL = $(OBJECTS_RISCOS)

OBJECTS_DEBUG = $(OBJECTS_COMMON) $(OBJECTS_IMAGE)
OBJECTS_DEBUG += debug_bitmap.o filetyped.o fontd.o netsurfd.o	# debug/

OBJECTS_DEBUGRO = $(OBJECTS_COMMON) $(OBJECTS_IMAGE)
OBJECTS_DEBUGRO += netsurfd.o					# debug/
OBJECTS_DEBUGRO += artworks.o awrender.o bitmap.o draw.o \
	filename.o filetype.o font.o gif.o gifread.o image.o \
	jpeg.o palettes.o plotters.o save_complete.o schedule.o \
	sprite.o						# riscos/

OBJECTS_GTK = $(OBJECTS_COMMON) $(OBJECTS_IMAGE)
OBJECTS_GTK += browser.o frames.o history_core.o netsurf.o \
	selection.o textinput.o 				# desktop/
OBJECTS_GTK += font_pango.o gtk_bitmap.o gtk_gui.o \
        gtk_schedule.o gtk_thumbnail.o gtk_options.o \
	gtk_plotters.o gtk_treeview.o gtk_scaffolding.o \
	gtk_completion.o gtk_login.o gtk_throbber.o \
	gtk_history.o gtk_window.o gtk_filetype.o \
	gtk_download.o						# gtk/


OBJDIR_RISCOS = $(shell $(CC) -dumpmachine)
SOURCES_RISCOS=$(OBJECTS_RISCOS:.o=.c)
OBJS_RISCOS=$(OBJECTS_RISCOS:%.o=$(OBJDIR_RISCOS)/%.o)

OBJDIR_RISCOS_SMALL = $(shell $(CC) -dumpmachine)-small
SOURCES_RISCOS_SMALL=$(OBJECTS_RISCOS_SMALL:.o=.c)
OBJS_RISCOS_SMALL=$(OBJECTS_RISCOS_SMALL:%.o=$(OBJDIR_RISCOS_SMALL)/%.o)

OBJDIR_DEBUG = $(shell $(SYSTEM_CC) -dumpmachine)-debug
SOURCES_DEBUG=$(OBJECTS_DEBUG:.o=.c)
OBJS_DEBUG=$(OBJECTS_DEBUG:%.o=$(OBJDIR_DEBUG)/%.o)

OBJS_DEBUGRO=$(OBJECTS_DEBUGRO:%.o=$(OBJDIR_RISCOS)/%.o)

OBJDIR_GTK = objects-gtk
SOURCES_GTK=$(OBJECTS_GTK:.o=.c)
OBJS_GTK=$(OBJECTS_GTK:%.o=$(OBJDIR_GTK)/%.o)

# Default target - platform specific files may specify special-case rules for
# various files.
default: riscos

# Inclusion of platform specific files has to occur after the OBJDIR stuff as
# that is referred to in the files

OS = $(word 2,$(subst -, ,$(shell $(SYSTEM_CC) -dumpmachine)))
ifeq ($(OS),riscos)
include riscos.mk
else
include posix.mk
endif

VPATH = content:content/fetchers:css:desktop:image:render:riscos:riscos/configure:riscos/gui:utils:debug:gtk

WARNFLAGS = -W -Wall -Wundef -Wpointer-arith -Wcast-qual \
	-Wcast-align -Wwrite-strings -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls \
	-Wnested-externs -Winline -Wno-unused-parameter -Wuninitialized

# CFLAGS have to appear after the inclusion of platform specific files as the
# PLATFORM_CFLAGS variables are defined in them

CFLAGS_RISCOS = -std=c99 -D_BSD_SOURCE -D_POSIX_C_SOURCE -Driscos -DBOOL_DEFINED -O \
	$(WARNFLAGS) -I. $(PLATFORM_CFLAGS_RISCOS) -mpoke-function-name \
#	-include utils/memdebug.h
CFLAGS_RISCOS_SMALL = $(CFLAGS_RISCOS) -Dsmall
CFLAGS_DEBUG = -std=c99 -D_BSD_SOURCE -DDEBUG_BUILD $(WARNFLAGS) -I. \
	$(PLATFORM_CFLAGS_DEBUG) -g
CFLAGS_GTK = -std=c99 -Dgtk -Dnsgtk \
	-DGTK_DISABLE_DEPRECATED \
	-D_BSD_SOURCE \
	-D_XOPEN_SOURCE=600 \
	-D_POSIX_C_SOURCE=200112L \
	$(WARNFLAGS) -I. -g -O \
	`pkg-config --cflags libglade-2.0 gtk+-2.0 librsvg-2.0` \
	`xml2-config --cflags`

# Stop GCC under Cygwin throwing a fit
# If you pass -std=<whatever> it appears to define __STRICT_ANSI__
# This causes use of functions such as vsnprintf to fail (as Cygwin's header
# files surround declarations of such things with #ifndef __STRICT_ANSI__)
ifneq ($(OS),riscos)
ifeq ($(shell echo $$OS),Windows_NT)
CFLAGS_GTK += -U__STRICT_ANSI__
endif
endif

AFLAGS_RISCOS = -xassembler-with-cpp $(PLATFORM_AFLAGS_RISCOS)
AFLAGS_RISCOS_SMALL = $(AFLAGS_RISCOS) -Dsmall

# targets
riscos: $(RUNIMAGE)
$(RUNIMAGE) : $(OBJS_RISCOS)
	$(CC) -o $@ $(LDFLAGS_RISCOS) $^
riscos_small: u!RunImage,ff8
u!RunImage,ff8 : $(OBJS_RISCOS_SMALL)
	$(CC) -o $@ $(LDFLAGS_SMALL) $^

debug: nsdebug
nsdebug: $(OBJS_DEBUG)
	$(CC_DEBUG) -o $@ $(LDFLAGS_DEBUG) $^

riscos_debug: nsrodebug,ff8
nsrodebug,ff8: $(OBJS_DEBUGRO)
	$(CC) -o $@ $(LDFLAGS_RISCOS) $^

gtk: nsgtk
nsgtk: $(OBJS_GTK)
	$(SYSTEM_CC) -o nsgtk $^ `pkg-config --cflags --libs libglade-2.0 gtk+-2.0 gthread-2.0 gmodule-2.0 librsvg-2.0` \
	$(LDFLAGS_DEBUG)

netsurf.zip: $(RUNIMAGE)
	rm netsurf.zip; riscos-zip -9vr, netsurf.zip !NetSurf

# pattern rules for c source
$(OBJDIR_RISCOS)/%.o : %.c
	@echo "==> $<"
	@$(CC) -o $@ -c $(CFLAGS_RISCOS) $<
$(OBJDIR_RISCOS_SMALL)/%.o : %.c
	@echo "==> $<"
	@$(CC) -o $@ -c $(CFLAGS_RISCOS_SMALL) $<
$(OBJDIR_DEBUG)/%.o : %.c
	@echo "==> $<"
	@$(CC_DEBUG) -o $@ -c $(CFLAGS_DEBUG) $<
$(OBJDIR_GTK)/%.o : %.c
	@echo "==> $<"
	@$(SYSTEM_CC) -o $@ -c $(CFLAGS_GTK) $<

# pattern rules for asm source
$(OBJDIR_RISCOS)/%.o : %.s
	@echo "==> $<"
	$(ASM) -o $@ -c $(AFLAGS_RISCOS) $<
$(OBJDIR_RISCOS_SMALL)/%.o : %.s
	@echo "==> $<"
	$(ASM) -o $@ -c $(AFLAGS_RISCOS_SMALL) $<

# Generate dependencies.
# To disable automatic regeneration of dependencies (eg. if perl is not
# available), remove */*.[ch] from the line below.
# Under RISC OS, you may require *Set UnixFS$sfix "", if perl gives
# "No such file or directory" errors.
depend: css/css_enum.c css/parser.c css/scanner.c utils/translit.c */*.[ch] */*/*.[ch]
	@echo "--> modified files $?"
	@echo "--> updating dependencies"
	@-mkdir -p $(OBJDIR_RISCOS) $(OBJDIR_RISCOS_SMALL) $(OBJDIR_DEBUG) $(OBJDIR_GTK)
	@perl scandeps $(OBJDIR_RISCOS) $(OBJDIR_RISCOS_SMALL) $(OBJDIR_DEBUG) $(OBJDIR_GTK) -- $^ > depend

include depend
