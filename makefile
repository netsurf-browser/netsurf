#
# This file is part of NetSurf, http://netsurf.sourceforge.net/
# Licensed under the GNU General Public License,
#                http://www.opensource.org/licenses/gpl-license
#

CC = riscos-gcc
CC_DEBUG = gcc
OBJECTS_COMMON = cache.o content.o fetch.o fetchcache.o other.o \
	css.o css_enum.o parser.o ruleset.o scanner.o \
	box.o html.o layout.o textplain.o \
	utils.o
OBJECTS = $(OBJECTS_COMMON) \
	browser.o netsurf.o \
	gif.o gui.o jpeg.o png.o theme.o plugin.o \
	options.o filetype.o font.o uri.o
OBJECTS_DEBUG = $(OBJECTS_COMMON) \
	netsurfd.o \
	optionsd.o filetyped.o fontd.o
DOCUMENTS = Themes.html	
VPATH = content:css:desktop:render:riscos:utils:debug
WARNFLAGS = -W -Wall -Wundef -Wpointer-arith -Wbad-function-cast -Wcast-qual \
	-Wcast-align -Wwrite-strings -Wconversion -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls \
	-Wnested-externs -Winline -Wno-unused-parameter -Wuninitialized
CFLAGS = -std=c9x -D_BSD_SOURCE -Driscos -DBOOL_DEFINED -O $(WARNFLAGS) -I.. -I/usr/local/riscoslibs/include \
	-Dfd_set=long -mpoke-function-name
CFLAGS_DEBUG = -std=c9x -D_BSD_SOURCE -O $(WARNFLAGS) -I.. -I/usr/include/libxml2 \
	-Dfd_set=long -g
LDFLAGS = \
	/usr/local/riscoslibs/animlib/animlib.ro \
	/usr/local/riscoslibs/libxml2/libxml2.ro \
	/usr/local/riscoslibs/OSLib/OSLib32.ro \
	/usr/local/riscoslibs/curl/libcurl.ro \
	/usr/local/riscoslibs/libpng/libpng.ro \
	/usr/local/riscoslibs/zlib/libz.ro \
	/usr/local/riscoslibs/openssl/libssl.a \
	/usr/local/riscoslibs/openssl/libcrypto.a
LDFLAGS_DEBUG = -L/usr/lib -lxml2 -lz -lm -lcurl -lssl -lcrypto -ldl

OBJDIR = $(shell $(CC) -dumpmachine)
SOURCES=$(OBJECTS:.o=.c)
OBJS=$(OBJECTS:%.o=$(OBJDIR)/%.o)
OBJDIR_DEBUG = $(shell $(CC_DEBUG) -dumpmachine)
SOURCES_DEBUG=$(OBJECTS_DEBUG:.o=.c)
OBJS_DEBUG=$(OBJECTS_DEBUG:%.o=$(OBJDIR_DEBUG)/%.o)
DOCDIR = !NetSurf/Docs
DOCS=$(DOCUMENTS:%.html=$(DOCDIR)/%.html)

# targets
all: !NetSurf/!RunImage,ff8 $(DOCS)
!NetSurf/!RunImage,ff8 : $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^
netsurf.zip: !NetSurf/!RunImage,ff8 $(DOCS)
	rm netsurf.zip; riscos-zip -9vr, netsurf.zip !NetSurf

# debug targets
debug: netsurf
netsurf: $(OBJS_DEBUG)
	$(CC_DEBUG) -o $@ $(LDFLAGS_DEBUG) $^

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
	
# create documentation
$(DOCDIR)/%.html: documentation/%.xml
	# syntax: xsltproc [options] -o <output file> <XSL stylesheet> <input file>
	# --nonet prevents connection to the web to find the stylesheet
	xsltproc -o $@ http://www.movspclr.co.uk/dtd/100/prm-html.xsl $<

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

