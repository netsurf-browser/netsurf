# $Id: makefile,v 1.17 2003/04/06 18:09:34 bursa Exp $

all: !NetSurf/!RunImage,ff8
clean:
	-rm */arm-riscos-aof/* css/parser.c css/parser.h css/scanner.c css/scanner.h

setup: render/arm-riscos-aof riscos/arm-riscos-aof desktop/arm-riscos-aof \
 content/arm-riscos-aof utils/arm-riscos-aof css/arm-riscos-aof
%/arm-riscos-aof:
	-mkdir $@

FLAGS = -Wall -W -Wundef -Wpointer-arith -Wbad-function-cast -Wcast-qual \
 -Wcast-align -Wwrite-strings -Wconversion -Wstrict-prototypes -Wmissing-prototypes \
 -Wmissing-declarations -Wredundant-decls -Wnested-externs -Winline \
 -I.. -I/usr/local/riscoslibs/include \
 -Dfd_set=long -mpoke-function-name -DNETSURF_DUMP
CC = riscos-gcc
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
 css/arm-riscos-aof/css.o css/arm-riscos-aof/css_enum.o \
 css/arm-riscos-aof/parser.o css/arm-riscos-aof/scanner.o \
 css/arm-riscos-aof/ruleset.o
HEADERS = \
 content/cache.h    content/content.h  content/fetch.h    content/fetchcache.h \
 desktop/browser.h  desktop/gui.h      desktop/netsurf.h  render/box.h \
 render/html.h      render/layout.h \
 riscos/font.h      riscos/gui.h       riscos/theme.h     utils/log.h \
 utils/utils.h      render/textplain.h \
 css/css.h css/css_enum.h css/parser.h css/scanner.h
LIBS = \
 /usr/local/riscoslibs/libxml2/libxml2.ro \
 /usr/local/riscoslibs/OSLib/OSLib.ro \
 /usr/local/riscoslibs/curl/libcurl.ro \
 /usr/local/riscoslibs/libutf-8/libutf-8.ro

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

utils/arm-riscos-aof/%.o: utils/%.c $(HEADERS) 
	$(CC) $(FLAGS) -o $@ -c $<

css/css_enum.c css/css_enum.h: css/css_enums css/makeenum
	cd ..; /usr/bin/perl netsurf/css/makeenum netsurf/css/css_enum < netsurf/css/css_enums

css/parser.c: css/parser.y
	-cd css; lemon parser.y

css/scanner.c css/scanner.h: css/scanner.l
	cd css; flex scanner.l

css/arm-riscos-aof/%.o: css/%.c $(HEADERS) 
	$(CC) $(FLAGS) -o $@ -c $<

netsurf.zip: !NetSurf/!RunImage,ff8
	rm netsurf.zip; riscos-zip -9vr, netsurf.zip !NetSurf

