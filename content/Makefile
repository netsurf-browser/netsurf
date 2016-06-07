# Content sources

S_CONTENT := content.c content_factory.c dirlist.c fetch.c hlcache.c	\
	llcache.c mimesniff.c urldb.c no_backing_store.c

# Make filesystem backing store available
ifeq ($(NETSURF_FS_BACKING_STORE),YES)
	S_CONTENT += fs_backing_store.c
endif


# Content fetchers sources
include content/fetchers/Makefile

# Content handlers
include content/handlers/Makefile

S_CONTENT := $(addprefix content/,$(S_CONTENT))
