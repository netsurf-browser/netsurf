CFLAGS := -std=c99 -g -O0 -D_BSD_SOURCE -D_POSIX_C_SOURCE -I.. \
		`pkg-config --cflags libxml-2.0 libcurl`
LDFLAGS := `pkg-config --libs libxml-2.0 libcurl`

llcache_CFLAGS := `pkg-config --cflags libparserutils`
llcache_LDFLAGS := `pkg-config --libs libparserutils`

llcache_SRCS := content/fetch.c content/fetchers/fetch_curl.c \
		content/fetchers/fetch_data.c content/llcache.c \
		content/urldb.c desktop/options.c desktop/version.c \
		utils/base64.c utils/hashtable.c utils/messages.c \
		utils/url.c utils/useragent.c utils/utf8.c utils/utils.c \
		test/llcache.c

urldbtest_SRCS := content/urldb.c utils/url.c utils/utils.c utils/messages.c \
		  utils/hashtable.c utils/filename.c test/urldbtest.c

urldbtest_CFLAGS := -O2
urldbtest_LDFLAGS :=

.PHONY: all

all: llcache urldbtest

llcache: $(addprefix ../,$(llcache_SRCS))
	$(CC) $(CFLAGS) $(llcache_CFLAGS) $^ -o $@ $(LDFLAGS) $(llcache_LDFLAGS)

urldbtest: $(addprefix ../,$(urldbtest_SRCS))
	$(CC) $(CFLAGS) $(urldbtest_CFLAGS) $^ -o $@ $(LDFLAGS) $(urldbtest_LDFLAGS)

.PHONY: clean

clean:
	$(RM) llcache urldbtest
