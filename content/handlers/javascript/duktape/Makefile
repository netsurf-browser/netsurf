#
# NetSurf javascript source file inclusion
#
# Included by javascript/Makefile
#

content/handlers/javascript/duktape/dukky.c: $(OBJROOT)/duktape/binding.h

BINDINGS := $(wildcard content/handlers/javascript/duktape/*.bnd)

# ensure genbind generates debugging files
GBFLAGS+=-D

$(OBJROOT)/duktape/binding.h $(OBJROOT)/duktape/Makefile: content/handlers/javascript/duktape/netsurf.bnd $(BINDINGS)
	$(Q)$(MKDIR) -p $(OBJROOT)/duktape
	$(VQ)echo " GENBIND: $<"
	$(Q)nsgenbind $(GBFLAGS) -I content/handlers/javascript/WebIDL $< $(OBJROOT)/duktape

# create unimplemented report for doxygen
Docs/UnimplementedJavascript.txt: content/handlers/javascript/duktape/netsurf.bnd $(BINDINGS)
	$(Q)$(MKDIR) -p $(OBJROOT)/duktape
	$(VQ)echo "/** \page unimplemented Unimplemented javascript bindings" > $@
	$(VQ)echo "This is a list of all the binding methods, getters and setters without an implementation in a binding." >> $@
	$(VQ)echo "" >> $@
	$(VQ)echo " GENBIND: $<"
	$(Q)nsgenbind $(GBFLAGS) -Wunimplemented -I content/handlers/javascript/WebIDL $< $(OBJROOT)/duktape 2>&1 >/dev/null | grep "Unimplemented" | cut -d' ' -f4- | sort -k 2 | awk '{print $$0"\\n" }' >> $@
	$(VQ)echo "*/" >> $@

ifeq ($(filter $(MAKECMDGOALS),clean test coverage),)
-include $(OBJROOT)/duktape/Makefile
endif

S_JAVASCRIPT_BINDING:=$(addprefix $(OBJROOT)/duktape/,$(NSGENBIND_SOURCES))

$(S_JAVASCRIPT_BINDING): $(BINDINGS)

S_JAVASCRIPT += content.c duktape/dukky.c duktape/duktape.c

CFLAGS += -DDUK_OPT_HAVE_CUSTOM_H
