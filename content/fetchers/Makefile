# Content fetchers sources

S_FETCHERS_YES := data.c about.c resource.c
S_FETCHERS_NO :=
S_FETCHERS_$(NETSURF_USE_CURL) += curl.c

S_FETCHERS := $(addprefix fetchers/,$(S_FETCHERS_YES))

# File fetcher
include content/fetchers/file/Makefile

S_FETCHERS += $(addprefix fetchers/file/,$(S_FETCHER_FILE))

# The following files depend on the testament
content/fetchers/about.c: testament $(OBJROOT)/testament.h
