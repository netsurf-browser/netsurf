# $Id: makefile,v 1.8 2002/10/15 11:09:56 bursa Exp $

all: !NetSurf/!RunImage,ff8
clean:
	rm */objs-riscos/*

FLAGS = -g -Wall -W -Wundef -Wpointer-arith -Wbad-function-cast -Wcast-qual \
 -Wcast-align -Wwrite-strings -Wconversion -Wstrict-prototypes -Wmissing-prototypes \
 -Wmissing-declarations -Wredundant-decls -Wnested-externs -Winline -std=c9x \
 -I.. -I../../Tools/libxml2/include -I../../Tools/oslib \
 -I../../Tools/curl/include -I../../Tools \
 -Dfd_set=long -mpoke-function-name -DNETSURF_DUMP
CC = riscos-gcc
OBJECTS = render/objs-riscos/utils.o render/objs-riscos/css.o \
 render/objs-riscos/css_enum.o render/objs-riscos/box.o \
 render/objs-riscos/layout.o \
 riscos/objs-riscos/gui.o riscos/objs-riscos/font.o \
 riscos/objs-riscos/theme.o \
 desktop/objs-riscos/browser.o desktop/objs-riscos/fetch.o \
 desktop/objs-riscos/netsurf.o
HEADERS = render/box.h render/css.h render/css_enum.h \
 render/layout.h render/utils.h riscos/font.h riscos/gui.h \
 riscos/theme.h \
 desktop/browser.h desktop/fetch.h desktop/gui.h desktop/netsurf.h
LIBS = ../../Tools/libxml2/libxml.ro ../../Tools/oslib/oslib.o \
 ../../Tools/curl/libcurl.ro ../../Tools/libutf-8/libutf-8.ro

!NetSurf/!RunImage,ff8: $(OBJECTS)
	$(CC) $(FLAGS) -o !NetSurf/!RunImage,ff8 $(OBJECTS) $(LIBS)

render/css_enum.c render/css_enum.h: render/css_enums render/makeenum
	cd ..; netsurf/render/makeenum netsurf/render/css_enum < netsurf/render/css_enums

render/objs-riscos/%.o: render/%.c $(HEADERS)
	$(CC) $(FLAGS) -o $@ -c $<

riscos/objs-riscos/%.o: riscos/%.c $(HEADERS) 
	$(CC) $(FLAGS) -o $@ -c $<

desktop/objs-riscos/%.o: desktop/%.c $(HEADERS) 
	$(CC) $(FLAGS) -o $@ -c $<

netsurf.zip: !NetSurf/!RunImage,ff8
	rm netsurf.zip; riscos-zip -9vr, netsurf.zip !NetSurf

