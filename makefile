# $Id: makefile,v 1.33 2003/06/05 14:39:54 bursa Exp $

CC = riscos-gcc
OBJECTS = cache.o content.o fetch.o fetchcache.o \
	css.o css_enum.o parser.o ruleset.o scanner.o \
	browser.o netsurf.o \
	box.o html.o layout.o textplain.o \
	filetype.o font.o gif.o gui.o jpeg.o png.o theme.o \
	utils.o plugin.o options.o
DOCUMENTS = Themes.html	
VPATH = content:css:desktop:render:riscos:utils
WARNFLAGS = -W -Wall -Wundef -Wpointer-arith -Wbad-function-cast -Wcast-qual \
	-Wcast-align -Wwrite-strings -Wconversion -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls \
	-Wnested-externs -Winline -Wno-unused-parameter
CFLAGS = $(WARNFLAGS) -I.. -I/usr/local/riscoslibs/include \
	-Dfd_set=long -mpoke-function-name
LDFLAGS = \
	/usr/local/riscoslibs/libungif/libungif.ro \
	/usr/local/riscoslibs/libxml2/libxml2.ro \
	/usr/local/riscoslibs/OSLib/OSLib32.ro \
	/usr/local/riscoslibs/curl/libcurl.ro \
	/usr/local/riscoslibs/libpng/libpng.ro \
	/usr/local/riscoslibs/zlib/libz.ro

OBJDIR = $(shell $(CC) -dumpmachine)
SOURCES=$(OBJECTS:.o=.c)
OBJS=$(OBJECTS:%.o=$(OBJDIR)/%.o)
DOCDIR = !NetSurf/Docs
DOCS=$(DOCUMENTS:%.html=$(DOCDIR)/%.html)

# targets
all: !NetSurf/!RunImage,ff8 $(DOCS)
!NetSurf/!RunImage,ff8 : $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^
netsurf.zip: !NetSurf/!RunImage,ff8 $(DOCS)
	rm netsurf.zip; riscos-zip -9vr, netsurf.zip !NetSurf

# pattern rule for c source
$(OBJDIR)/%.o : %.c
	$(CC) -o $@ -c $(CFLAGS) $(CFLAGS) $<

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
depend : $(SOURCES)
	-mkdir $(OBJDIR)
	$(CC) -MM -MG $(CFLAGS) $^ | sed 's|.*\.o:|$(OBJDIR)/&|g' > $@

# remove generated files
clean :
	-rm $(OBJDIR)/* depend css/css_enum.c css/css_enum.h \
		css/parser.c css/parser.h css/scanner.c css/scanner.h

include depend

