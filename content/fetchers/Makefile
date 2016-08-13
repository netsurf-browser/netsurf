# Content fetchers sources

S_FETCHERS_YES := data.c file.c about.c resource.c
S_FETCHERS_NO :=
S_FETCHERS_$(NETSURF_USE_CURL) += curl.c

S_FETCHERS := $(addprefix content/fetchers/,$(S_FETCHERS_YES))

# The following files depend on the testament
content/fetchers/about.c: testament $(OBJROOT)/testament.h
