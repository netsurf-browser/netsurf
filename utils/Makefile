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

# Host tool to convert file to comiled data
#
$(TOOLROOT)/xxd: utils/xxd.c $(TOOLROOT)/created
	$(VQ)echo "BUILD CC: $@"
	$(Q)$(BUILD_CC) $(BUILD_CFLAGS) -o $@ $< $(BUILD_LDFLAGS)

# Host tool to convert file to comiled data
#
$(TOOLROOT)/split-messages: utils/split-messages.c $(TOOLROOT)/created
	$(VQ)echo "BUILD CC: $@"
	$(Q)$(BUILD_CC) $(BUILD_CFLAGS) -o $@ $< $(BUILD_LDFLAGS) -lz
