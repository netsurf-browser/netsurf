# $Id: makefile,v 1.22 2003/05/31 18:50:28 jmb Exp $

CC = riscos-gcc
<<<<<<< makefile
OBJECTS = \
 content/arm-riscos-aof/cache.o  content/arm-riscos-aof/content.o \
 content/arm-riscos-aof/fetch.o  content/arm-riscos-aof/fetchcache.o \
 desktop/arm-riscos-aof/browser.o  desktop/arm-riscos-aof/netsurf.o \
 render/arm-riscos-aof/box.o \
 render/arm-riscos-aof/html.o \
 render/arm-riscos-aof/layout.o render/arm-riscos-aof/textplain.o \
 riscos/arm-riscos-aof/font.o  riscos/arm-riscos-aof/gui.o \
 riscos/arm-riscos-aof/theme.o riscos/arm-riscos-aof/jpeg.o \
 riscos/arm-riscos-aof/filetype.o utils/arm-riscos-aof/utils.o \
 riscos/arm-riscos-aof/png.o riscos/arm-riscos-aof/plugin.o \
 css/arm-riscos-aof/css.o css/arm-riscos-aof/css_enum.o \
 css/arm-riscos-aof/parser.o css/arm-riscos-aof/scanner.o \
 css/arm-riscos-aof/ruleset.o
HEADERS = \
 content/cache.h    content/content.h  content/fetch.h    content/fetchcache.h \
 desktop/browser.h  desktop/gui.h      desktop/netsurf.h  render/box.h \
 render/html.h      render/layout.h \
 riscos/font.h      riscos/gui.h       riscos/theme.h     utils/log.h \
 utils/utils.h      render/textplain.h \
 css/css.h css/css_enum.h css/parser.h css/scanner.h \
 riscos/png.h riscos/plugin.h
LIBS = \
 /usr/local/riscoslibs/libxml2/libxml2.ro \
 /usr/local/riscoslibs/OSLib/OSLib32.ro \
 /usr/local/riscoslibs/curl/libcurl.ro \
 /usr/local/riscoslibs/libpng/libpng.ro \
 /usr/local/riscoslibs/zlib/libz.ro

!NetSurf/!RunImage,ff8: $(OBJECTS)
	$(CC) $(FLAGS) -o !NetSurf/!RunImage,ff8 $(OBJECTS) $(LIBS)

render/arm-riscos-aof/%.o: render/%.c $(HEADERS)
	$(CC) $(FLAGS) -o $@ -c $<

riscos/arm-riscos-aof/%.o: riscos/%.c $(HEADERS) 
	$(CC) $(FLAGS) -o $@ -c $<

desktop/arm-riscos-aof/%.o: desktop/%.c $(HEADERS) 
	$(CC) $(FLAGS) -o $@ -c $<

content/arm-riscos-aof/%.o: content/%.c $(HEADERS) 
	$(CC) $(FLAGS) -o $@ -c $<
=======
OBJECTS = cache.o content.o fetch.o fetchcache.o \
	css.o css_enum.o parser.o ruleset.o scanner.o \
	browser.o netsurf.o \
	box.o html.o layout.o textplain.o \
	filetype.o font.o gui.o jpeg.o png.o theme.o \
	utils.o
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
>>>>>>> 1.21

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
$(OBJDIR) :
	-mkdir $(OBJDIR)

# generate dependencies
depend : $(SOURCES)
	$(CC) -MM -MG $(CFLAGS) $^ | sed 's|.*\.o:|$(OBJDIR)/& $(OBJDIR)|g' > $@

# remove generated files
clean :
	-rm $(OBJDIR)/* depend css/css_enum.c css/css_enum.h \
		css/parser.c css/parser.h css/scanner.c css/scanner.h

include depend

