#
# NetSurf unit tests


test_CFLAGS := -std=c99 -g -Wall \
		-D_BSD_SOURCE \
		-D_POSIX_C_SOURCE=200809L \
		-D_XOPEN_SOURCE=600 \
		-Itest -I. -I.. \
		$(shell pkg-config --cflags libcurl)
test_LDFLAGS := $(shell pkg-config --libs libcurl) -lz

# nsurl sources and flags
nsurl_SRCS := utils/corestrings.c utils/nsurl.c utils/idna.c test/log.c test/nsurl.c
nsurl_CFLAGS := $(shell pkg-config --cflags libwapcaplet libdom libutf8proc) -O0
nsurl_LDFLAGS := $(shell pkg-config --libs libwapcaplet libdom libutf8proc)

# low level cache sources and flags
llcache_SRCS := content/fetch.c content/fetchers/curl.c \
		content/fetchers/about.c content/fetchers/data.c \
		content/fetchers/resource.c content/llcache.c \
		content/urldb.c desktop/version.c \
		image/image_cache.c \
		utils/base64.c utils/corestrings.c utils/hashtable.c \
		utils/nsurl.c utils/messages.c utils/url.c \
		utils/useragent.c utils/utils.c test/llcache.c
llcache_CFLAGS := $(shell pkg-config --cflags libparserutils libwapcaplet libdom) -O2
llcache_LDFLAGS := $(shell pkg-config --libs libparserutils libwapcaplet libdom)

# url database test sources and flags
urldbtest_SRCS := content/urldb.c utils/url.c utils/utils.c utils/idna.c \
		utils/messages.c utils/hashtable.c utils/bloom.c utils/nsoption.c \
		utils/filename.c utils/nsurl.c utils/corestrings.c \
		test/log.c test/urldbtest.c
urldbtest_CFLAGS := $(shell pkg-config --cflags libwapcaplet libdom libnsutils libutf8proc) -O2
urldbtest_LDFLAGS := $(shell pkg-config --libs libwapcaplet libdom libnsutils libutf8proc)


nsoption_SRCS := utils/log.c utils/nsoption.c test/nsoption.c
nsoption_CFLAGS := -Dnsgtk

CLEANS += test-clean

TESTS := nsurl urldbtest

TESTROOT := build-$(HOST)-test


.PHONY:test

test: $(TESTROOT)/created $(addprefix $(TESTROOT)/,$(TESTS))
	$(TESTROOT)/nsurl
	$(TESTROOT)/urldbtest

$(TESTROOT)/created:
	$(VQ)echo "   MKDIR: $(TESTROOT)"
	$(Q)$(MKDIR) $(TESTROOT)
	$(Q)$(TOUCH) $@

$(TESTROOT)/nsurl: $(nsurl_SRCS)
	$(CC) $(test_CFLAGS) $(nsurl_CFLAGS) $^ -o $@ $(test_LDFLAGS) $(nsurl_LDFLAGS)


$(TESTROOT)/urldbtest: $(urldbtest_SRCS)
	$(CC) $(test_CFLAGS) $(urldbtest_CFLAGS) $^ -o $@ $(test_LDFLAGS) $(urldbtest_LDFLAGS)

$(TESTROOT)/llcache: $(llcache_SRCS)
	$(CC) $(test_CFLAGS) $(llcache_CFLAGS) $^ -o $@ $(test_LDFLAGS) $(llcache_LDFLAGS)



$(TESTROOT)/nsoption: $(addprefix ../,$(nsoption_SRCS))
	$(CC) $(test_CFLAGS) $(nsoption_CFLAGS) $^ -o $@ $(test_LDFLAGS) $(nsoption_LDFLAGS)

.PHONY: test-clean

test-clean:
	$(RM) $(addprefix $(TESTROOT)/,$(TESTS))
