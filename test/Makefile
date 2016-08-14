#
# NetSurf unit tests

TESTS := \
	nsurl \
	urldbtest \
	nsoption \
	bloom \
	hashtable \
	urlescape \
	utils \
	messages \
	time #llcache

# nsurl sources
nsurl_SRCS := utils/corestrings.c utils/nsurl.c utils/idna.c \
		test/log.c test/nsurl.c

# url database test sources
urldbtest_SRCS := content/urldb.c \
		utils/idna.c utils/bloom.c utils/nsoption.c utils/nsurl.c \
		utils/corestrings.c \
		utils/hashtable.c utils/messages.c utils/time.c utils/utils.c \
		test/log.c test/urldbtest.c

# low level cache sources
llcache_SRCS := content/fetch.c content/fetchers/curl.c \
		content/fetchers/about.c content/fetchers/data.c \
		content/fetchers/resource.c content/llcache.c \
		content/urldb.c \
		image/image_cache.c \
		utils/base64.c utils/corestrings.c utils/hashtable.c \
		utils/nsurl.c utils/messages.c utils/url.c utils/useragent.c \
		utils/utils.c \
		test/log.c test/llcache.c

# messages test sources
messages_SRCS := utils/messages.c utils/hashtable.c test/log.c test/messages.c

# nsoption test sources
nsoption_SRCS := utils/nsoption.c test/log.c test/nsoption.c

# Bloom filter test sources
bloom_SRCS := utils/bloom.c test/bloom.c

# hash table test sources
hashtable_SRCS := utils/hashtable.c test/log.c test/hashtable.c

# url escape test sources
urlescape_SRCS := utils/url.c test/log.c test/urlescape.c

# utility test sources
utils_SRCS := utils/utils.c utils/messages.c utils/hashtable.c \
		utils/corestrings.c utils/nsurl.c utils/idna.c \
		test/log.c test/utils.c

# time test sources
time_SRCS := utils/time.c test/log.c test/time.c

# Coverage builds need additional flags
ifeq ($(MAKECMDGOALS),coverage)
  COV_CFLAGS ?= -fprofile-arcs -ftest-coverage -O0
  COV_CXXFLAGS ?= -fprofile-arcs -ftest-coverage -O0
  COV_LDFLAGS ?= -lgcov -fprofile-arcs
  TESTROOT := build-$(HOST)-coverage
else
  COV_CFLAGS ?= -O0
  COV_CXXFLAGS ?= -O0
  TESTROOT := build-$(HOST)-test
endif


# Extend flags with appropriate values from pkg-config for enabled features
#
# 1: pkg-config required modules for feature
# 2: Human-readable name for the feature
define pkg_cfg_detect_lib
  ifeq ($$(PKG_CONFIG),)
    $$(error pkg-config is required to auto-detect feature availability)
  endif

  PKG_CONFIG_$(1)_EXISTS := $$(shell $$(PKG_CONFIG) --exists $(1) && echo yes)

  ifeq ($$(PKG_CONFIG_$(1)_EXISTS),yes)
      LIB_CFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(1))
      LIB_CXXFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(1))
      LIB_LDFLAGS += $$(shell $$(PKG_CONFIG) --libs $(1))
      ifneq ($(MAKECMDGOALS),clean)
        $$(info PKG.CNFG: $(2) ($(1))	enabled)
      endif
  else
    ifneq ($(MAKECMDGOALS),clean)
      $$(info PKG.CNFG: $(2) ($(1))	disabled)
    endif
  endif
endef


$(eval $(call pkg_cfg_detect_lib,check,Check))

COMMON_WARNFLAGS = -W -Wall -Wundef -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wmissing-declarations -Wuninitialized

ifneq ($(CC_MAJOR),2)
  COMMON_WARNFLAGS += -Wno-unused-parameter
endif

TESTCFLAGS := -std=c99 -g \
		$(COMMON_WARNFLAGS) \
		-D_BSD_SOURCE \
		-D_POSIX_C_SOURCE=200809L \
		-D_XOPEN_SOURCE=600 \
		-Itest -Iinclude -Icontent/handlers -Ifrontends -I. -I.. \
		-Dnsgtk \
		$(shell pkg-config --cflags libcurl libparserutils libwapcaplet libdom libnsutils libutf8proc libidn) \
		$(LIB_CFLAGS) \
		$(COV_CFLAGS)

TESTLDFLAGS := $(shell pkg-config --libs libcurl libparserutils libwapcaplet libdom libnsutils libutf8proc libidn) -lz \
		$(LIB_LDFLAGS)\
		$(COV_LDFLAGS)


# Source files for all tests being compiled
TESTSOURCES :=

GCOV ?= gcov

define gen_test_target
$$(TESTROOT)/$(1): $$(sort $$(addprefix $$(TESTROOT)/,$$(subst /,_,$$(patsubst %.c,%.o,$$(patsubst %.cpp,%.o,$$(patsubst %.m,%.o,$$(patsubst %.s,%.o,$$($(1)_SRCS))))))))
	$$(VQ)echo "LINKTEST: $$@"
	$$(Q)$$(CC) $$(TESTCFLAGS) $$^ -o $$@ $$(TESTLDFLAGS)

.PHONY:$(1)_test

$(1)_test:$$(TESTROOT)/$(1)
	$$(VQ)echo "RUN TEST: $(1)"
	$$(Q)$$(TESTROOT)/$(1)

TESTSOURCES += $$($(1)_SRCS)

endef

define compile_test_target_c
$$(TESTROOT)/$(2): $(1) $$(TESTROOT)/created
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(RM) $$(TESTROOT)/$(2)
	$$(Q)$$(CC) $$(TESTCFLAGS) -o $$(TESTROOT)/$(2) -c $(1)

endef

# Generate target for each test program and the list of objects it needs
$(eval $(foreach TST,$(TESTS), $(call gen_test_target,$(TST))))

# generate target rules for test objects
$(eval $(foreach SOURCE,$(sort $(filter %.c,$(TESTSOURCES))), \
	$(call compile_test_target_c,$(SOURCE),$(subst /,_,$(SOURCE:.c=.o)),$(subst /,_,$(SOURCE:.c=.d)))))


.PHONY:test coverage

test: $(TESTROOT)/created $(addsuffix _test,$(TESTS))

coverage: test

$(TESTROOT)/created:
	$(VQ)echo "   MKDIR: $(TESTROOT)"
	$(Q)$(MKDIR) $(TESTROOT)
	$(Q)$(TOUCH) $@

.PHONY: test-clean

test-clean:
	$(VQ)echo "   CLEAN: $(TESTROOT)"
	$(VQ)echo "   CLEAN: build-$(HOST)-coverage"
	$(Q)$(RM) -r $(TESTROOT) build-$(HOST)-coverage
CLEANS += test-clean
