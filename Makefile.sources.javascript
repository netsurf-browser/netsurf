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

# 1: input file
# 2: output file
# 3: binding name
define convert_jsapi_binding

S_JSAPI_BINDING += $(2)
D_JSAPI_BINDING += $(patsubst %.c,%.d,$(2))

$(2): $(1) $(OBJROOT)/created
	$$(VQ)echo " GENBIND: $(1)"
	$(Q)nsgenbind -I javascript/WebIDL -d $(patsubst %.c,%.d,$(2)) -o $(2) $(1)

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

S_JAVASCRIPT += content.c jsapi.c $(addprefix jsapi/,$(S_JSAPI))

$(eval $(foreach V,$(filter JSAPI_BINDING_%,$(.VARIABLES)),$(call convert_jsapi_binding,$($(V)),$(OBJROOT)/$(patsubst JSAPI_BINDING_%,%,$(V)).c,$(patsubst JSAPI_BINDING_%,%,$(V))_jsapi)))


else
S_JAVASCRIPT += none.c
endif
