#
# NetSurf javascript source file inclusion
#
# Included by Makefile
#

# Check if jsapi is required
ifeq ($(NETSURF_USE_JS),YES)
WANT_JS_SOURCE := YES
else
ifeq ($(NETSURF_USE_MOZJS),YES)
WANT_JS_SOURCE := YES
endif
endif

S_JAVASCRIPT_BINDING:=

ifeq ($(WANT_JS_SOURCE),YES)
# JSAPI (spidemonkey)
include javascript/jsapi/Makefile
else
ifeq ($(NETSURF_USE_DUKTAPE),YES)
# Duktape
include javascript/duktape/Makefile
else
# None
include javascript/none/Makefile
endif
endif

# Fetcher for javascript scheme is always required
S_JAVASCRIPT += fetcher.c

S_JAVASCRIPT := $(addprefix javascript/,$(S_JAVASCRIPT)) $(S_JAVASCRIPT_BINDING)
