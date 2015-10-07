#
# NetSurf javascript source file inclusion
#
# Included by Makefile.sources 
#

# ----------------------------------------------------------------------------
# JSAPI binding
# ----------------------------------------------------------------------------

S_JSAPI_BINDING:=
D_JSAPI_BINDING:=

JSAPI_BINDING_htmldocument := javascript/jsapi/htmldocument.bnd
JSAPI_BINDING_htmlelement := javascript/jsapi/htmlelement.bnd
JSAPI_BINDING_window := javascript/jsapi/window.bnd
JSAPI_BINDING_navigator := javascript/jsapi/navigator.bnd
JSAPI_BINDING_console := javascript/jsapi/console.bnd
JSAPI_BINDING_location := javascript/jsapi/location.bnd
JSAPI_BINDING_htmlcollection := javascript/jsapi/htmlcollection.bnd
JSAPI_BINDING_nodelist := javascript/jsapi/nodelist.bnd
JSAPI_BINDING_text := javascript/jsapi/text.bnd
JSAPI_BINDING_comment := javascript/jsapi/comment.bnd
JSAPI_BINDING_node := javascript/jsapi/node.bnd
JSAPI_BINDING_event := javascript/jsapi/event.bnd

# 1: input binding file
# 2: source output file
# 3: header output file
# 4: binding name
define convert_jsapi_binding

S_JSAPI_BINDING += $(2)
D_JSAPI_BINDING += $(patsubst %.c,%.d,$(2))

$(2): $(1) $(OBJROOT)/created
	$$(VQ)echo " GENBIND: $(1)"
	$(Q)nsgenbind -g -I javascript/WebIDL -d $(patsubst %.c,%.d,$(2)) -h $(3) -o $(2) $(1)

$(3): $(2)

endef

# Javascript sources
ifeq ($(NETSURF_USE_JS),YES)
WANT_JS_SOURCE := YES
endif

ifeq ($(NETSURF_USE_MOZJS),YES)
WANT_JS_SOURCE := YES
endif

ifeq ($(WANT_JS_SOURCE),YES)

S_JSAPI :=

S_JAVASCRIPT += content.c jsapi.c fetcher.c $(addprefix jsapi/,$(S_JSAPI))

$(eval $(foreach V,$(filter JSAPI_BINDING_%,$(.VARIABLES)),$(call convert_jsapi_binding,$($(V)),$(OBJROOT)/$(patsubst JSAPI_BINDING_%,%,$(V)).c,$(OBJROOT)/$(patsubst JSAPI_BINDING_%,%,$(V)).h,$(patsubst JSAPI_BINDING_%,%,$(V))_jsapi)))

ifeq ($(filter $(MAKECMDGOALS),clean test coverage),)
-include $(D_JSAPI_BINDING)
endif

else
ifeq ($(NETSURF_USE_DUKTAPE),YES)

javascript/dukky.c: $(OBJROOT)/duktape/binding.h

BINDINGS := $(wildcard javascript/duktape/*.bnd)

$(OBJROOT)/duktape/binding.h $(OBJROOT)/duktape/Makefile: javascript/duktape/netsurf.bnd $(BINDINGS)
	$(Q)mkdir -p $(OBJROOT)/duktape
	$(VQ)echo " GENBIND: $<"
	$(Q)nsgenbind -D -g -I javascript/WebIDL $< $(OBJROOT)/duktape

# create unimplemented report for doxygen
Docs/UnimplementedJavascript.txt: javascript/duktape/netsurf.bnd $(BINDINGS)
	$(Q)mkdir -p $(OBJROOT)/duktape
	$(VQ)echo "/** \page unimplemented Unimplemented javascript bindings" > $@
	$(VQ)echo "" >> $@
	$(VQ)echo " GENBIND: $<"
	$(Q)nsgenbind -Wunimplemented -I javascript/WebIDL $< $(OBJROOT)/duktape 2>&1 >/dev/null | grep "Unimplemented" | cut -d' ' -f4- | sort -k 2 | awk '{print $$0"\\n" }' >> $@
	$(VQ)echo "*/" >> $@

ifeq ($(filter $(MAKECMDGOALS),clean test coverage),)
-include $(OBJROOT)/duktape/Makefile
endif

S_JSAPI_BINDING:=$(addprefix $(OBJROOT)/duktape/,$(NSGENBIND_SOURCES))

$(S_JSAPI_BINDING): $(BINDINGS)

S_JAVASCRIPT += dukky.c content.c fetcher.c duktape/duktape.c

CFLAGS += -DDUK_OPT_HAVE_CUSTOM_H
else
S_JAVASCRIPT += none.c fetcher.c
endif
endif

S_JAVASCRIPT := $(addprefix javascript/,$(S_JAVASCRIPT)) $(S_JSAPI_BINDING)
