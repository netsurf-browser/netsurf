#
# This file is part of NetSurf, http://netsurf.sourceforge.net/
# Licensed under the GNU General Public License,
#                http://www.opensource.org/licenses/gpl-license
#

CC = /riscos/bin/gcc
CC_DEBUG = gcc
OBJECTS_COMMON = cache.o content.o fetch.o fetchcache.o \
	css.o css_enum.o parser.o ruleset.o scanner.o \
	box.o form.o html.o layout.o textplain.o \
	messages.o utils.o translit.o pool.o url.o
OBJECTS = $(OBJECTS_COMMON) \
	browser.o loginlist.o netsurf.o options.o \
	htmlinstance.o htmlredraw.o \
	401login.o constdata.o dialog.o download.o frames.o gui.o \
	menus.o mouseactions.o \
	textselection.o theme.o window.o \
	draw.o gif.o jpeg.o plugin.o png.o sprite.o \
	about.o filetype.o font.o uri.o url_protocol.o history.o \
	version.o thumbnail.o \
	save.o save_complete.o save_draw.o schedule.o
OBJECTS_DEBUG = $(OBJECTS_COMMON) \
	netsurfd.o \
	options.o filetyped.o fontd.o
OBJECTS_DEBUGRO = $(OBJECTS_COMMON) \
	netsurfd.o \
	constdata.o \
	theme.o \
	draw.o gif.o jpeg.o png.o sprite.o \
	about.o filetype.o \
	version.o \
	options.o font.o
VPATH = content:css:desktop:render:riscos:utils:debug
WARNFLAGS = -W -Wall -Wundef -Wpointer-arith -Wbad-function-cast -Wcast-qual \
	-Wcast-align -Wwrite-strings -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls \
	-Wnested-externs -Winline -Wno-unused-parameter -Wuninitialized
CFLAGS = -std=c9x -D_BSD_SOURCE -Driscos -DBOOL_DEFINED -O $(WARNFLAGS) -I.. \
	-mpoke-function-name
CFLAGS_DEBUG = -std=c9x -D_BSD_SOURCE $(WARNFLAGS) -I.. -I/usr/include/libxml2 -g
LDFLAGS = -L/riscos/lib -lxml2 -lz -lcurl -lssl -lcrypto -lares -lanim -lpng \
	-loslib -ljpeg
LDFLAGS_SMALL = -L/riscos/lib -lxml2 -lz -lucurl -lares -lanim -lpng -loslib -ljpeg
LDFLAGS_DEBUG = -L/usr/lib -lxml2 -lz -lm -lcurl -lssl -lcrypto -ldl

OBJDIR = $(shell $(CC) -dumpmachine)
SOURCES=$(OBJECTS:.o=.c)
OBJS=$(OBJECTS:%.o=$(OBJDIR)/%.o)
OBJDIR_DEBUG = $(shell $(CC_DEBUG) -dumpmachine)
SOURCES_DEBUG=$(OBJECTS_DEBUG:.o=.c)
OBJS_DEBUG=$(OBJECTS_DEBUG:%.o=$(OBJDIR_DEBUG)/%.o)
OBJS_DEBUGRO=$(OBJECTS_DEBUGRO:%.o=$(OBJDIR)/%.o)

# targets
all: !NetSurf/!RunImage,ff8 $(DOCS)
!NetSurf/!RunImage,ff8 : $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^
u!RunImage,ff8 : $(OBJS)
	$(CC) -o $@ $(LDFLAGS_SMALL) $^
netsurf.zip: !NetSurf/!RunImage,ff8 $(DOCS)
	rm netsurf.zip; riscos-zip -9vr, netsurf.zip !NetSurf

# debug targets
debug: netsurf
netsurf: $(OBJS_DEBUG)
	$(CC_DEBUG) -o $@ $(LDFLAGS_DEBUG) $^
debugro: nsdebug,ff8
nsdebug,ff8: $(OBJS_DEBUGRO)
	$(CC) -o $@ $(LDFLAGS) $^

# pattern rule for c source
$(OBJDIR)/%.o : %.c
	$(CC) -o $@ -c $(CFLAGS) $<
$(OBJDIR_DEBUG)/%.o : %.c
	$(CC_DEBUG) -o $@ -c $(CFLAGS_DEBUG) $<

# special cases
css/css_enum.c css/css_enum.h: css/css_enums css/makeenum
	cd ..; /usr/bin/perl netsurf/css/makeenum netsurf/css/css_enum < netsurf/css/css_enums
css/parser.c: css/parser.y
	-cd css; lemon parser.y
css/scanner.c css/scanner.h: css/scanner.l
	cd css; flex scanner.l
utils/translit.c: transtab
	cd utils; ./tt2code < transtab > translit.c

# generate dependencies
depend : $(SOURCES) $(SOURCES_DEBUG)
	-mkdir $(OBJDIR) $(OBJDIR_DEBUG)
	$(CC) -MM -MG $(CFLAGS) $^ | sed 's|.*\.o:|$(OBJDIR)/&|g' > $@
	$(CC_DEBUG) -MM -MG $(CFLAGS_DEBUG) $^ | sed 's|.*\.o:|$(OBJDIR_DEBUG)/&|g' >> $@

# remove generated files
clean :
	-rm $(OBJDIR)/* $(OBJDIR_DEBUG)/* depend css/css_enum.c css/css_enum.h \
		css/parser.c css/parser.h css/scanner.c css/scanner.h

include depend

