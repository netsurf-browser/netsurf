# $Id: makefile,v 1.26 2003/06/01 23:44:38 jmb Exp $

CC = riscos-gcc
OBJECTS = cache.o content.o fetch.o fetchcache.o \
	css.o css_enum.o parser.o ruleset.o scanner.o \
	browser.o netsurf.o \
	box.o html.o layout.o textplain.o \
	filetype.o font.o gui.o jpeg.o png.o theme.o \
	utils.o plugin.o options.o
VPATH = content:css:desktop:render:riscos:utils
WARNFLAGS = -W -Wall -Wundef -Wpointer-arith -Wbad-function-cast -Wcast-qual \
	-Wcast-align -Wwrite-strings -Wconversion -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls \
	-Wnested-externs -Winline -Wno-unused-parameter
CFLAGS = $(WARNFLAGS) -I.. -I/usr/local/riscoslibs/include \
	-Dfd_set=long -mpoke-function-name
LDFLAGS = \
	/usr/local/riscoslibs/libxml2/libxml2.ro \
	/usr/local/riscoslibs/OSLib/OSLib32.ro \
	/usr/local/riscoslibs/curl/libcurl.ro \
	/usr/local/riscoslibs/libpng/libpng.ro \
	/usr/local/riscoslibs/zlib/libz.ro

OBJDIR = $(shell $(CC) -dumpmachine)
SOURCES=$(OBJECTS:.o=.c)
OBJS=$(OBJECTS:%.o=$(OBJDIR)/%.o)

# targets
!NetSurf/!RunImage,ff8 : $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^
netsurf.zip: !NetSurf/!RunImage,ff8
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

# generate dependencies
depend : $(SOURCES)
	-mkdir $(OBJDIR)
	$(CC) -MM -MG $(CFLAGS) $^ | sed 's|.*\.o:|$(OBJDIR)/&|g' > $@

# remove generated files
clean :
	-rm $(OBJDIR)/* depend css/css_enum.c css/css_enum.h \
		css/parser.c css/parser.h css/scanner.c css/scanner.h

include depend

