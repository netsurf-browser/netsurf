CFLAGS := -std=c99 -g -O0 -D_BSD_SOURCE -D_POSIX_C_SOURCE -I.. \
		$(shell pkg-config --cflags libxml-2.0 libcurl)
LDFLAGS := $(shell pkg-config --libs libxml-2.0 libcurl) -lz

llcache_CFLAGS := $(shell pkg-config --cflags libparserutils libwapcaplet)
llcache_LDFLAGS := $(shell pkg-config --libs libparserutils libwapcaplet)

llcache_SRCS := content/fetch.c content/fetchers/curl.c \
		content/fetchers/about.c content/fetchers/data.c \
		content/fetchers/resource.c content/llcache.c \
		content/urldb.c desktop/options.c desktop/version.c \
		image/image_cache.c \
		utils/base64.c utils/hashtable.c utils/log.c utils/nsurl.c \
		utils/messages.c utils/url.c utils/useragent.c utils/utf8.c \
		utils/utils.c test/llcache.c

urldbtest_SRCS := content/urldb.c utils/url.c utils/utils.c utils/log.c \
		utils/messages.c utils/hashtable.c utils/filename.c \
		test/urldbtest.c

urldbtest_CFLAGS := -O2
urldbtest_LDFLAGS :=

nsurl_SRCS := utils/log.c utils/nsurl.c test/nsurl.c
nsurl_CFLAGS := $(shell pkg-config --cflags libwapcaplet)
nsurl_LDFLAGS := $(shell pkg-config --libs libwapcaplet)

.PHONY: all

all: llcache urldbtest nsurl

llcache: $(addprefix ../,$(llcache_SRCS))
	$(CC) $(CFLAGS) $(llcache_CFLAGS) $^ -o $@ $(LDFLAGS) $(llcache_LDFLAGS)

urldbtest: $(addprefix ../,$(urldbtest_SRCS))
	$(CC) $(CFLAGS) $(urldbtest_CFLAGS) $^ -o $@ $(LDFLAGS) $(urldbtest_LDFLAGS)

nsurl: $(addprefix ../,$(nsurl_SRCS))
	$(CC) $(CFLAGS) $(nsurl_CFLAGS) $^ -o $@ $(LDFLAGS) $(nsurl_LDFLAGS)

.PHONY: clean

clean:
	$(RM) llcache urldbtest nsurl
