#
# NetSurf javascript source file inclusion
#
# Included by Makefile.sources 
#

# ----------------------------------------------------------------------------
# JSAPI binding
# ----------------------------------------------------------------------------

S_JSAPI_BINDING:=

JSAPI_BINDING_htmldocument := javascript/jsapi/bindings/htmldocument.bnd
JSAPI_BINDING_window := javascript/jsapi/bindings/window.bnd
JSAPI_BINDING_navigator := javascript/jsapi/bindings/navigator.bnd

# 1: input file
# 2: output file
# 3: binding name
define convert_jsapi_binding

S_JSAPI_BINDING += $(2)

$(2): $(1)
	$$(VQ)echo " GENBIND: $(1)"
	$(Q)nsgenbind -I javascript/WebIDL/ -o $(2) $(1)

endef

# Javascript sources
ifeq ($(NETSURF_USE_JS),YES)

S_JSAPI =  console.c htmlelement.c
#htmldocument.c window.c navigator.c

S_JAVASCRIPT += content.c jsapi.c $(addprefix jsapi/,$(S_JSAPI))

$(eval $(foreach V,$(filter JSAPI_BINDING_%,$(.VARIABLES)),$(call convert_jsapi_binding,$($(V)),$(OBJROOT)/$(patsubst JSAPI_BINDING_%,%,$(V)).c,$(patsubst JSAPI_BINDING_%,%,$(V))_jsapi)))


else
S_JAVASCRIPT += none.c
endif
