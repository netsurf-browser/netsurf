#
# Makefile for NetSurf
#
# Copyright 2007 Daniel Silverstone <dsilvers@netsurf-browser.org>
#
#

# Trivially, invoke as:
#   make
# to build native, or:
#   make TARGET=riscos
# to cross-build for RO.
#
# Tested on unix platforms (building for GTK and cross-compiling for RO) and
# on RO (building for RO).
#
# To clean, invoke as above, with the 'clean' target
#
# To build developer Doxygen generated documentation, invoke as above,
# with the 'docs' target:
#   make docs
#

all: all-program

# Determine host type
# NOTE: Currently, this is broken on RISC OS due to what appear to
#       be bugs in UnixLib's pipe()/dup2() implementations. Until these
#       are fixed and a new build of make is available, manually hardcode 
#       this to "riscos" (sans quotes). Please remember to change it back
#       to "$(shell uname -s)" (sans quotes) again before committing any
#       Makefile changes.
HOST := $(shell uname -s)

ifeq ($(HOST),riscos)
# Build happening on RO platform, default target is RO backend
ifeq ($(TARGET),)
TARGET := riscos
endif
else
# Build happening on non-RO platform, default target is GTK backend
ifeq ($(TARGET),)
TARGET := gtk
endif
endif

Q=@
VQ=@
PERL=perl
MKDIR=mkdir
TOUCH=touch

OBJROOT := $(HOST)-$(TARGET)

$(OBJROOT)/created:
	$(VQ)echo "   MKDIR: $(OBJROOT)"
	$(Q)$(MKDIR) $(OBJROOT)
	$(Q)$(TOUCH) $(OBJROOT)/created

DEPROOT := $(OBJROOT)/deps
$(DEPROOT)/created: $(OBJROOT)/created
	$(VQ)echo "   MKDIR: $(DEPROOT)"
	$(Q)$(MKDIR) $(DEPROOT)
	$(Q)$(TOUCH) $(DEPROOT)/created

WARNFLAGS = -W -Wall -Wundef -Wpointer-arith -Wcast-qual \
	-Wcast-align -Wwrite-strings -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls \
	-Wnested-externs -Winline -Wno-unused-parameter -Wuninitialized

ifeq ($(TARGET),riscos)
ifeq ($(HOST),riscos)
# Build for RO on RO
CC := gcc
EXEEXT :=
else
# Cross-build for RO
CC := /home/riscos/cross/bin/gcc
EXEEXT := ,ff8
endif
STARTGROUP :=
ENDGROUP :=
else
STARTGROUP := -Wl,--start-group
ENDGROUP := -Wl,--end-group
endif

LDFLAGS := -lxml2 -lz -lm -lcurl -lssl -lcrypto -lmng \
	-ljpeg

ifeq ($(TARGET),gtk)
# Building for GTK, we need the GTK flags

GTKCFLAGS := -std=c99 -Dgtk -Dnsgtk \
	-DGTK_DISABLE_DEPRECATED \
	-D_BSD_SOURCE \
	-D_XOPEN_SOURCE=600 \
	-D_POSIX_C_SOURCE=200112L \
	-D_NETBSD_SOURCE \
	$(WARNFLAGS) -I. -g -O \
	$(shell pkg-config --cflags libglade-2.0 gtk+-2.0 librsvg-2.0) \
	$(shell xml2-config --cflags)

GTKLDFLAGS := $(shell pkg-config --cflags --libs libglade-2.0 gtk+-2.0 gthread-2.0 gmodule-2.0 librsvg-2.0)
CFLAGS += $(GTKCFLAGS)
LDFLAGS += $(GTKLDFLAGS) -llcms

ifeq ($(HOST),Windows_NT)
CFLAGS += -U__STRICT_ANSI__
endif

endif

ifeq ($(TARGET),riscos)
ifeq ($(HOST),riscos)
GCCSDK_INSTALL_ENV := <NSLibs$$Dir>
else
GCCSDK_INSTALL_ENV := /home/riscos/env
endif

CFLAGS += -I. -O $(WARNFLAGS) -Driscos			\
	-std=c99 -D_BSD_SOURCE -D_POSIX_C_SOURCE	\
	-mpoke-function-name

CFLAGS += -I$(GCCSDK_INSTALL_ENV)/include		\
	-I$(GCCSDK_INSTALL_ENV)/include/libxml2		\
	-I$(GCCSDK_INSTALL_ENV)/include/libmng
ifeq ($(HOST),riscos)
CFLAGS += -I<OSLib$$Dir> -mthrowback
endif
ASFLAGS += -I. -I$(GCCSDK_INSTALL_ENV)/include
LDFLAGS += -L$(GCCSDK_INSTALL_ENV)/lib -lcares -lrufl -lpencil \
	-lsvgtiny
ifeq ($(HOST),riscos)
LDFLAGS += -LOSLib: -lOSLib32
else
LDFLAGS += -lOSLib32
endif
endif

CLEANS := clean-target

include Makefile.sources

OBJECTS := $(sort $(addprefix $(OBJROOT)/,$(subst /,_,$(patsubst %.c,%.o,$(patsubst %.s,%.o,$(SOURCES))))))

$(EXETARGET): $(OBJECTS)
	$(VQ)echo "    LINK: $(EXETARGET)"
	$(Q)$(CC) -o $(EXETARGET) $(STARTGROUP) $(OBJECTS) $(ENDGROUP) $(LDFLAGS)

clean-target:
	$(VQ)echo "   CLEAN: $(EXETARGET)"
	$(Q)$(RM) $(EXETARGET)

clean-builddir:
	$(VQ)echo "   CLEAN: $(OBJROOT)"
	$(Q)$(RM) -r $(OBJROOT)
CLEANS += clean-builddir

all-program: $(EXETARGET)

.SUFFIXES:

DEPFILES :=
# Now some macros which build the make system

# 1 = Source file
# 2 = dep filename, no prefix
# 3 = obj filename, no prefix
define dependency_generate_c
DEPFILES += $(2)
$$(DEPROOT)/$(2): $$(DEPROOT)/created $(1) css/css_enum.h css/parser.h
	$$(VQ)echo "     DEP: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(2)
	$$(Q)$$(CC) $$(CFLAGS) -MM -MT '$$(DEPROOT)/$2 $$(OBJROOT)/$(3)' \
		    -MF $$(DEPROOT)/$(2) $(1)

endef

# 1 = Source file
# 2 = dep filename, no prefix
# 3 = obj filename, no prefix
define dependency_generate_s
DEPFILES += $(2)
$$(DEPROOT)/$(2): $$(DEPROOT)/created $(1)
	$$(VQ)echo "     DEP: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(2)
	$$(Q)$$(CC) $$(CFLAGS) -MM -MT '$$(DEPROOT)/$2 $$(OBJROOT)/$(3)' \
		    -MF $$(DEPROOT)/$(2) $(1)

endef

# 1 = Source file
# 2 = obj filename, no prefix
# 3 = dep filename, no prefix
define compile_target_c
$$(OBJROOT)/$(2): $$(OBJROOT)/created $$(DEPROOT)/$(3)
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(CC) $$(CFLAGS) -o $$@ -c $(1)

endef

# 1 = Source file
# 2 = obj filename, no prefix
# 3 = dep filename, no prefix
define compile_target_s
$$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo " ASSEMBLE: $(1)"
	$$(Q)$$(CC) $$(ASFLAGS) -o $$@ -c $(1)

endef

# Rules to construct dep lines for each object...
$(eval $(foreach SOURCE,$(filter %.c,$(SOURCES)), \
	$(call dependency_generate_c,$(SOURCE),$(subst /,_,$(SOURCE:.c=.d)),$(subst /,_,$(SOURCE:.c=.o)))))

# Cannot currently generate dep files for S files because they're objasm
# when we move to gas format, we will be able to.

#$(eval $(foreach SOURCE,$(filter %.s,$(SOURCES)), \
#	$(call dependency_generate_s,$(SOURCE),$(subst /,_,$(SOURCE:.s=.d)),$(subst /,_,$(SOURCE:.s=.o)))))

ifneq ($(MAKECMDGOALS),clean)
-include $(sort $(addprefix $(DEPROOT)/,$(DEPFILES)))
endif

# And rules to build the objects themselves...

$(eval $(foreach SOURCE,$(filter %.c,$(SOURCES)), \
	$(call compile_target_c,$(SOURCE),$(subst /,_,$(SOURCE:.c=.o)),$(subst /,_,$(SOURCE:.c=.d)))))

$(eval $(foreach SOURCE,$(filter %.s,$(SOURCES)), \
	$(call compile_target_s,$(SOURCE),$(subst /,_,$(SOURCE:.s=.o)),$(subst /,_,$(SOURCE:.s=.d)))))


clean: $(CLEANS)

docs:
	doxygen Docs/Doxyfile
