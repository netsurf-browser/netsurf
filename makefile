# $Id: makefile,v 1.10 2002/12/30 14:30:20 bursa Exp $

all: dirs !NetSurf/!RunImage,ff8
clean:
	rm */arm-riscos-aof/*
dirs: render/arm-riscos-aof riscos/arm-riscos-aof desktop/arm-riscos-aof
%/arm-riscos-aof:
	mkdir $@

FLAGS = -g -Wall -W -Wundef -Wpointer-arith -Wbad-function-cast -Wcast-qual \
 -Wcast-align -Wwrite-strings -Wconversion -Wstrict-prototypes -Wmissing-prototypes \
 -Wmissing-declarations -Wredundant-decls -Wnested-externs -Winline -std=c9x \
 -I.. -I/usr/local/riscoslibs/include \
 -Dfd_set=long -mpoke-function-name -DNETSURF_DUMP
CC = riscos-gcc
OBJECTS = render/arm-riscos-aof/utils.o render/arm-riscos-aof/css.o \
 render/arm-riscos-aof/css_enum.o render/arm-riscos-aof/box.o \
 render/arm-riscos-aof/layout.o \
 riscos/arm-riscos-aof/gui.o riscos/arm-riscos-aof/font.o \
 riscos/arm-riscos-aof/theme.o \
 desktop/arm-riscos-aof/browser.o desktop/arm-riscos-aof/fetch.o \
 desktop/arm-riscos-aof/netsurf.o desktop/arm-riscos-aof/cache.o
HEADERS = render/box.h render/css.h render/css_enum.h \
 render/layout.h render/utils.h riscos/font.h riscos/gui.h \
 riscos/theme.h \
 desktop/browser.h desktop/fetch.h desktop/gui.h desktop/netsurf.h \
 desktop/cache.h
LIBS = \
 /usr/local/riscoslibs/libxml2/libxml2.ro \
 /usr/local/riscoslibs/OSLib/OSLib.ro \
 /usr/local/riscoslibs/curl/libcurl.ro \
 /usr/local/riscoslibs/libutf-8/libutf-8.ro \
 /usr/local/riscoslibs/ubiqx/ubiqx.ro

!NetSurf/!RunImage,ff8: $(OBJECTS)
	$(CC) $(FLAGS) -o !NetSurf/!RunImage,ff8 $(OBJECTS) $(LIBS)

render/css_enum.c render/css_enum.h: render/css_enums render/makeenum
	cd ..; /usr/bin/perl netsurf/render/makeenum netsurf/render/css_enum < netsurf/render/css_enums

render/arm-riscos-aof/%.o: render/%.c $(HEADERS)
	$(CC) $(FLAGS) -o $@ -c $<

riscos/arm-riscos-aof/%.o: riscos/%.c $(HEADERS) 
	$(CC) $(FLAGS) -o $@ -c $<

desktop/arm-riscos-aof/%.o: desktop/%.c $(HEADERS) 
	$(CC) $(FLAGS) -o $@ -c $<

netsurf.zip: !NetSurf/!RunImage,ff8
	rm netsurf.zip; riscos-zip -9vr, netsurf.zip !NetSurf

