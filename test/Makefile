CFLAGS := -std=c99 -g -O0 -D_BSD_SOURCE -D_POSIX_C_SOURCE -I.. \
		`pkg-config --cflags libxml-2.0 libcurl libparserutils`
LDFLAGS := `pkg-config --libs libxml-2.0 libcurl libparserutils`

llcache_SRCS := content/fetch.c content/fetchers/fetch_curl.c \
		content/fetchers/fetch_data.c content/llcache.c \
		content/urldb.c desktop/options.c desktop/version.c \
		utils/base64.c utils/hashtable.c utils/messages.c \
		utils/url.c utils/useragent.c utils/utf8.c utils/utils.c \
		test/llcache.c

llcache: $(addprefix ../,$(llcache_SRCS))
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)


.PHONY: clean

clean:
	$(RM) llcache
