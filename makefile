# $Id: makefile,v 1.1 2002/07/27 21:10:45 bursa Exp $

all: netsurf,ff8
clean:
	rm */objs-riscos/*

FLAGS = -g -Wall -W -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual \
 -Wcast-align -Wwrite-strings -Wconversion -Wstrict-prototypes -Wmissing-prototypes \
 -Wmissing-declarations -Wredundant-decls -Wnested-externs -Winline -std=c9x \
 -I.. -I../../Tools/libxml2/include -I../../Tools/oslib \
 -I../../Tools/curl/include -Dfd_set=long
CC = riscos-gcc
OBJECTS = render/objs-riscos/utils.o render/objs-riscos/css.o \
 render/objs-riscos/css_enum.o render/objs-riscos/box.o \
 render/objs-riscos/layout.o \
 riscos/objs-riscos/netsurf.o riscos/objs-riscos/font.o
HEADERS = render/box.h render/css.h render/css_enum.h render/font.h \
 render/layout.h render/utils.h
LIBS = ../../Tools/libxml2/libxml.ro ../../Tools/oslib/oslib.o ../../Tools/curl/libcurl.ro

netsurf,ff8: $(OBJECTS)
	$(CC) $(FLAGS) -o netsurf,ff8 $(OBJECTS) $(LIBS)

render/css_enum.c render/css_enum.h: render/css_enums render/makeenum
	render/makeenum render/css_enum < render/css_enums

render/objs-riscos/%.o: render/%.c $(HEADERS)
	$(CC) $(FLAGS) -o $@ -c $<

riscos/objs-riscos/%.o: riscos/%.c $(HEADERS) 
	$(CC) $(FLAGS) -o $@ -c $<
