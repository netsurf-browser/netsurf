# $Id: makefile,v 1.3 2002/05/04 19:57:18 bursa Exp $

FLAGS = -g -Wall -W -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual \
-Wcast-align -Wwrite-strings -Wconversion -Wstrict-prototypes -Wmissing-prototypes \
-Wmissing-declarations -Wredundant-decls -Wnested-externs -Winline -std=c9x `xml2-config --cflags`
CC = gcc

render: render.o utils.o css.o css_enum.o font.o box.o layout.o
	$(CC) $(FLAGS) -o render render.o utils.o css.o css_enum.o font.o box.o layout.o `xml2-config --libs` 

render.o: render.c css.h css_enum.h utils.h font.h box.h layout.h
	$(CC) $(FLAGS) -c render.c

css.o: css.c css.h css_enum.h utils.h
	$(CC) $(FLAGS) -c css.c

utils.o: utils.c utils.h
	$(CC) $(FLAGS) -c utils.c

font.o: font.c font.h
	$(CC) $(FLAGS) -c font.c

css_enum.o: css_enum.c css_enum.h
	$(CC) $(FLAGS) -c css_enum.c

css_enum.c css_enum.h: css_enums makeenum
	./makeenum css_enum < css_enums

box.o: box.c box.h font.h css.h utils.h
	$(CC) $(FLAGS) -c box.c

layout.o: layout.c layout.h font.h css.h utils.h box.h
	$(CC) $(FLAGS) -c layout.c

