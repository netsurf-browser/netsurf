# $Id: makefile,v 1.2 2002/04/25 15:49:32 bursa Exp $

FLAGS = -g -Wall -W -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual \
-Wcast-align -Wwrite-strings -Wconversion -Wstrict-prototypes -Wmissing-prototypes \
-Wmissing-declarations -Wredundant-decls -Wnested-externs -Winline -std=c9x
CC = gcc

render: render.o utils.o css.o css_enum.o font.o
	$(CC) $(FLAGS) -o render render.o utils.o css.o css_enum.o font.o `xml2-config --libs` 

render.o: render.c css.h css_enum.h utils.h font.h
	$(CC) $(FLAGS) `xml2-config --cflags` -c render.c

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
