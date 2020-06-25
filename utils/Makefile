# utils sources

S_UTILS := \
	bloom.c \
	corestrings.c \
	file.c \
	filename.c \
	filepath.c \
	hashmap.c \
	hashtable.c \
	idna.c \
	libdom.c \
	log.c \
	messages.c \
	nscolour.c \
	nsoption.c \
	punycode.c \
	ssl_certs.c \
	talloc.c \
	time.c \
	url.c \
	useragent.c \
	utf8.c \
	utils.c

S_UTILS := $(addprefix utils/,$(S_UTILS))


# lib png build compiler flags
ifeq ($(HOST),OpenBSD)
    BUILD_LIBPNG_CFLAGS += $(shell $(PKG_CONFIG) --cflags libpng)
    BUILD_LIBPNG_LDFLAGS += $(shell $(PKG_CONFIG) --libs libpng)
else
  ifeq ($(HOST),FreeBSD)
      BUILD_LIBPNG_CFLAGS += $(shell $(PKG_CONFIG) --cflags libpng)
      BUILD_LIBPNG_LDFLAGS += $(shell $(PKG_CONFIG) --libs libpng)
  else
      BUILD_LIBPNG_CFLAGS +=
      BUILD_LIBPNG_LDFLAGS += -lpng
  endif
endif


# Build tool to convert file to comiled data
#
$(TOOLROOT)/xxd: utils/xxd.c $(TOOLROOT)/created
	$(VQ)echo "BUILD CC: $@"
	$(Q)$(BUILD_CC) $(BUILD_CFLAGS) -o $@ $< $(BUILD_LDFLAGS)


# Build tool to filter messages
#
$(TOOLROOT)/split-messages: utils/split-messages.c $(TOOLROOT)/created
	$(VQ)echo "BUILD CC: $@"
	$(Q)$(BUILD_CC) $(BUILD_CFLAGS) -o $@ $< $(BUILD_LDFLAGS) -lz


# Build tool to convert image bitmaps to source code.
#
$(TOOLROOT)/convert_image: utils/convert_image.c $(TOOLROOT)/created
	$(VQ)echo "BUILD CC: $@"
	$(Q)$(BUILD_CC) $(BUILD_CFLAGS) $(BUILD_LIBPNG_CFLAGS) -o $@ $< $(BUILD_LDFLAGS) $(BUILD_LIBPNG_LDFLAGS)


# Build too to perform font conversion
#
$(TOOLROOT)/convert_font: utils/convert_font.c $(TOOLROOT)/created
	$(VQ)echo "BUILD CC: $@"
	$(Q)$(BUILD_CC) $(BUILD_CFLAGS) -o $@ $<


# idna
#
IDNA_UNICODE_MAJOR=11

utils/DerivedJoiningType.txt:
	curl -o $@ "https://www.unicode.org/Public/$(IDNA_UNICODE_MAJOR).0.0/ucd/extracted/DerivedJoiningType.txt"

utils/IdnaMappingTable.txt:
	curl -o $@ "https://www.unicode.org/Public/idna/$(IDNA_UNICODE_MAJOR).0.0/IdnaMappingTable.txt"

utils/idna-tables-properties.csv:
	curl -o $@ "https://www.iana.org/assignments/idna-tables-$(IDNA_UNICODE_MAJOR).0.0/idna-tables-properties.csv"

# the idna props header must be explicitly rebuilt
ifneq ($(filter $(MAKECMDGOALS),utils/idna_props.h),)
utils/idna_props.h: utils/DerivedJoiningType.txt utils/idna-tables-properties.csv
	$(VQ)echo "    IDNA: $@"
	$(Q)$(PERL) utils/idna-derived-props-gen.pl -o $@ -p utils/idna-tables-properties.csv -j utils/DerivedJoiningType.txt
endif
