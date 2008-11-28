# Makefile for CSS test code

AR := ar
CC := gcc
LD := gcc 

CFLAGS := -O2 -I.. -I/usr/include/libxml2 -std=c99
LDFLAGS := -lxml2 -lz -lcurl

LIBOBJS := css.o css_enum.o parser.o ruleset.o scanner.o \
	messages.o hashtable.o talloc.o url.o utils.o

.PHONY: default lib clean

default:
	$(error "You probably wanted 'make test'")

test: lib testcss.o
	$(LD) -o $@ testcss.o $(LDFLAGS) -L. -lnscss

lib: $(LIBOBJS)
	$(AR) -cru libnscss.a $^

clean:
	$(RM) $(LIBOBJS) testcss.o test libnscss.a

messages.o: ../utils/messages.c 
	$(CC) $(CFLAGS) -c -o $@ $<

hashtable.o: ../utils/hashtable.c 
	$(CC) $(CFLAGS) -c -o $@ $<

talloc.o: ../utils/talloc.c
	$(CC) $(CFLAGS) -c -o $@ $<

url.o: ../utils/url.c 
	$(CC) $(CFLAGS) -c -o $@ $<

utils.o: ../utils/utils.c 
	$(CC) $(CFLAGS) -c -o $@ $<

%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

