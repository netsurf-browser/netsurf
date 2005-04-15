#
# This file is part of NetSurf, http://netsurf.sourceforge.net/
# Licensed under the GNU General Public License,
#                http://www.opensource.org/licenses/gpl-license
#

# There are 6 possible builds of NetSurf:
#
#   riscos -- standard RISC OS build
#   riscos_small -- identical to "riscos", but linked with smaller libraries
#   		(no openssl, and libcurl without ssl support)
#   ncos -- NCOS build (variant of RISC OS for Network Computers)
#   debug -- command line Unix/Linux, for debugging
#   riscos_debug -- a cross between "riscos" and "debug"
#   gtk -- experimental gtk version
#
# "riscos", "riscos_small", "ncos", and "riscos_debug" can be compiled under
# RISC OS, or cross-compiled using gccsdk.

OBJECTS_COMMON = content.o fetch.o fetchcache.o	url_store.o	# content/
OBJECTS_COMMON += css.o css_enum.o parser.o ruleset.o scanner.o	# css/
OBJECTS_COMMON += box.o box_construct.o box_normalise.o form.o html.o \
	html_redraw.o layout.o list.o textplain.o		# render/
OBJECTS_COMMON += messages.o pool.o talloc.o url.o utils.o	# utils/
OBJECTS_COMMON += imagemap.o loginlist.o options.o selection.o \
	textinput.o tree.o					# desktop/

OBJECTS_IMAGE = jpeg.o mng.o gif.o gifread.o			# image/

OBJECTS_RISCOS = $(OBJECTS_COMMON) $(OBJECTS_IMAGE)
OBJECTS_RISCOS += browser.o netsurf.o version.o			# desktop/
OBJECTS_RISCOS += 401login.o bitmap.o buffer.o debugwin.o \
	dialog.o download.o draw.o filetype.o font.o \
	global_history.o gui.o help.o history.o hotlist.o image.o \
	menus.o mouseactions.o plotters.o plugin.o print.o \
	save.o save_complete.o save_draw.o save_text.o \
	schedule.o search.o sprite.o textselection.o theme.o \
	theme_install.o thumbnail.o treeview.o ucstables.o uri.o \
	url_complete.o url_protocol.o wimp.o window.o		# riscos/
# OBJECTS_RISCOS += memdebug.o

OBJECTS_NCOS = $(OBJECTS_RISCOS)

OBJECTS_DEBUG = $(OBJECTS_COMMON) $(OBJECTS_IMAGE)
OBJECTS_DEBUG += debug_bitmap.o filetyped.o fontd.o netsurfd.o	# debug/

OBJECTS_DEBUGRO = $(OBJECTS_COMMON) $(OBJECTS_IMAGE)
OBJECTS_DEBUGRO += netsurfd.o					# debug/
OBJECTS_DEBUGRO += version.o					# desktop/
OBJECTS_DEBUGRO += bitmap.o draw.o filetype.o font.o \
	gif.o gifread.o image.o jpeg.o plotters.o save_complete.o \
	schedule.o sprite.o ufont.o				# riscos/

OBJECTS_GTK = $(OBJECTS_COMMON) $(OBJECTS_IMAGE)
OBJECTS_GTK += filetyped.o					# debug/
OBJECTS_GTK += browser.o netsurf.o version.o			# desktop/
OBJECTS_GTK += font_pango.o gtk_bitmap.o gtk_gui.o \
	gtk_plotters.o gtk_treeview.o gtk_window.o		# gtk/


OBJDIR_RISCOS = arm-riscos-aof
SOURCES_RISCOS=$(OBJECTS_RISCOS:.o=.c)
OBJS_RISCOS=$(OBJECTS_RISCOS:%.o=$(OBJDIR_RISCOS)/%.o)

OBJDIR_NCOS = arm-ncos-aof
SOURCES_NCOS=$(OBJECTS_NCOS:.o=.c)
OBJS_NCOS=$(OBJECTS_NCOS:%.o=$(OBJDIR_NCOS)/%.o)

OBJDIR_DEBUG = $(shell $(CC_DEBUG) -dumpmachine)-debug
SOURCES_DEBUG=$(OBJECTS_DEBUG:.o=.c)
OBJS_DEBUG=$(OBJECTS_DEBUG:%.o=$(OBJDIR_DEBUG)/%.o)

OBJS_DEBUGRO=$(OBJECTS_DEBUGRO:%.o=$(OBJDIR_RISCOS)/%.o)

OBJDIR_GTK = $(shell gcc -dumpmachine)-gtk
SOURCES_GTK=$(OBJECTS_GTK:.o=.c)
OBJS_GTK=$(OBJECTS_GTK:%.o=$(OBJDIR_GTK)/%.o)

# Inclusion of platform specific files has to occur after the OBJDIR stuff as
# that is refered to in the files

OS = $(word 2,$(subst -, ,$(shell gcc -dumpmachine)))
ifeq ($(OS),riscos)
include riscos.mk
else
include posix.mk
endif

VPATH = content:css:desktop:image:render:riscos:utils:debug:gtk

WARNFLAGS = -W -Wall -Wundef -Wpointer-arith -Wcast-qual \
	-Wcast-align -Wwrite-strings -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls \
	-Wnested-externs -Winline -Wno-unused-parameter -Wuninitialized

# CFLAGS have to appear after the inclusion of platform specific files as the
# PLATFORM_CFLAGS variables are defined in them

CFLAGS_RISCOS = -std=c9x -D_BSD_SOURCE -Driscos -DBOOL_DEFINED -O \
	$(WARNFLAGS) -I.. $(PLATFORM_CFLAGS_RISCOS) -mpoke-function-name \
#	-include netsurf/utils/memdebug.h
CFLAGS_NCOS = $(CFLAGS_RISCOS) -Dncos
CFLAGS_DEBUG = -std=c9x -D_BSD_SOURCE -Ddebug $(WARNFLAGS) -I.. \
	$(PLATFORM_CFLAGS_DEBUG) -g
CFLAGS_GTK = -std=c9x -D_BSD_SOURCE -D_POSIX_C_SOURCE -Dgtk \
	$(WARNFLAGS) -I.. -g \
	`pkg-config --cflags gtk+-2.0` `xml2-config --cflags`

AFLAGS_RISCOS = -CPU XScale -ThrowBack -Quit

# targets
riscos: $(RUNIMAGE)
$(RUNIMAGE) : $(OBJS_RISCOS)
	$(CC) -o $@ $(LDFLAGS_RISCOS) $^
riscos_small: u!RunImage,ff8
u!RunImage,ff8 : $(OBJS_RISCOS)
	$(CC) -o $@ $(LDFLAGS_SMALL) $^

ncos: $(NCRUNIMAGE)
$(NCRUNIMAGE) : $(OBJS_NCOS)
	$(CC) -o $@ $(LDFLAGS_RISCOS) $^

debug: nsdebug
nsdebug: $(OBJS_DEBUG)
	$(CC_DEBUG) -o $@ $(LDFLAGS_DEBUG) $^

riscos_debug: nsrodebug,ff8
nsrodebug,ff8: $(OBJS_DEBUGRO)
	$(CC) -o $@ $(LDFLAGS_RISCOS) $^

gtk: nsgtk
nsgtk: $(OBJS_GTK)
	gcc -o nsgtk `pkg-config --cflags --libs gtk+-2.0 gthread-2.0` \
	$(LDFLAGS_DEBUG) $^

netsurf.zip: $(RUNIMAGE)
	rm netsurf.zip; riscos-zip -9vr, netsurf.zip !NetSurf

# pattern rules for c source
$(OBJDIR_RISCOS)/%.o : %.c
	@echo "==> $<"
	@$(CC) -o $@ -c $(CFLAGS_RISCOS) $<
$(OBJDIR_NCOS)/%.o : %.c
	@echo "==> $<"
	@$(CC) -o $@ -c $(CFLAGS_NCOS) $<
$(OBJDIR_DEBUG)/%.o : %.c
	@echo "==> $<"
	@$(CC_DEBUG) -o $@ -c $(CFLAGS_DEBUG) $<
$(OBJDIR_GTK)/%.o : %.c
	@echo "==> $<"
	@gcc -o $@ -c $(CFLAGS_GTK) $<

# pattern rules for asm source
$(OBJDIR_RISCOS)/%.o : %.s
	@echo "== $<"
	$(ASM) -o $@ $(AFLAGS_RISCOS) $<

# special cases
css/css_enum.c css/css_enum.h: css/css_enums css/makeenum
	cd ..; perl netsurf/css/makeenum netsurf/css/css_enum < netsurf/css/css_enums
css/parser.c css/parser.h: css/parser.y
	-cd css; lemon parser.y
css/scanner.c: css/scanner.l
	cd css; re2c -s scanner.l > scanner.c
utils/translit.c: transtab
	cd utils; perl tt2code < transtab > translit.c

# Generate dependencies.
# To disable automatic regeneration of dependencies (eg. if perl is not
# available), remove */*.[ch] from the line below.
# Under RISC OS, you may require *Set UnixFS$sfix "", if perl gives
# "No such file or directory" errors.
depend: */*.[ch]
	@echo "--> modified files $?"
	@echo "--> updating dependencies"
	@-mkdir -p $(OBJDIR_RISCOS) $(OBJDIR_NCOS) $(OBJDIR_DEBUG) $(OBJDIR_GTK)
	@perl scandeps netsurf $(OBJDIR_RISCOS) $(OBJDIR_NCOS) $(OBJDIR_DEBUG) $(OBJDIR_GTK) -- $^ > depend

include depend

# remove generated files
clean:
	-rm $(OBJDIR_RISCOS)/* $(OBJDIR_NCOS)/* \
		$(OBJDIR_DEBUG)/* $(OBJDIR_GTK)/* \
		css/css_enum.c css/css_enum.h \
		css/parser.c css/parser.h css/scanner.c css/scanner.h
