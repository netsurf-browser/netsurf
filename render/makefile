# $Id: makefile,v 1.1.1.1 2002/04/22 09:24:34 bursa Exp $

FLAGS = -g -Wall -W -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wcast-qual \
-Wcast-align -Wwrite-strings -Wconversion -Wstrict-prototypes -Wmissing-prototypes \
-Wmissing-declarations -Wredundant-decls -Wnested-externs -Winline -std=c9x
CC = gcc

render: render.o utils.o css.o css_enum.o
	$(CC) $(FLAGS) `xml2-config --libs` -o render render.o utils.o css.o css_enum.o

render.o: render.c css.h css_enum.h utils.h
	$(CC) $(FLAGS) `xml2-config --cflags` -c render.c

css.o: css.c css.h css_enum.h utils.h
	$(CC) $(FLAGS) -c css.c

utils.o: utils.c utils.h
	$(CC) $(FLAGS) -c utils.c

css_enum.o: css_enum.c css_enum.h
	$(CC) $(FLAGS) -c css_enum.c

css_enum.c css_enum.h: css_enums makeenum
	./makeenum css_enum < css_enums
