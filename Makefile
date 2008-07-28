#
# Makefile for NetSurf
#
# Copyright 2007 Daniel Silverstone <dsilvers@netsurf-browser.org>
# Copyright 2008 Rob Kendrick <rjek@netsurf-browser.org>
#
# Trivially, invoke as:
#   make
# to build native, or:
#   make TARGET=riscos
# to cross-build for RO.
#
# Look at Makefile.config for configuration options.
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
# NOTE: HOST determination on RISC OS could fail because of missing bug fixes
#	in UnixLib which only got addressed in UnixLib 5 / GCCSDK 4.
#	When you don't have 'uname' available, you will see:
#	  File 'uname' not found
#	When you do and using a 'uname' compiled with a buggy UnixLib, you
#	will see the following printed on screen:
#	  RISC OS
#	In both cases HOST make variable is empty and we recover from that by
#	assuming we're building on RISC OS.
#	In case you don't see anything printed (including the warning), you
#	have an update to date RISC OS build system. ;-)
HOST := $(shell uname -s)
ifeq ($(HOST),)
HOST := riscos
$(warning Build platform determination failed but that's a known problem for RISC OS so we're assuming a native RISC OS build.)
else
ifeq ($(HOST),RISC OS)
# Fixup uname -s returning "RISC OS"
HOST := riscos
endif
endif

ifeq ($(HOST),riscos)
# Build happening on RO platform, default target is RO backend
ifeq ($(TARGET),)
TARGET := riscos
endif
else
ifeq ($(HOST),BeOS)
HOST := beos
endif
ifeq ($(HOST),Haiku)
# Haiku implements the BeOS API
HOST := beos
endif
ifeq ($(HOST),beos)
# Build happening on BeOS platform, default target is BeOS backend
ifeq ($(TARGET),)
TARGET := beos
endif
# BeOS still uses gcc2
GCCVER := 2
else
# Build happening on non-RO platform, default target is GTK backend
ifeq ($(TARGET),)
TARGET := gtk
endif
endif
endif
SUBTARGET =
RESOURCES =

ifneq ($(TARGET),riscos)
ifneq ($(TARGET),gtk)
ifneq ($(TARGET),beos)
ifneq ($(TARGET),debug)
$(error Unknown TARGET "$(TARGET)", should either be "riscos", "gtk", "beos" or "debug")
endif
endif
endif
endif

Q=@
VQ=@
PERL=perl
MKDIR=mkdir
TOUCH=touch

ifeq ($(TARGET),riscos)
ifeq ($(HOST),riscos)
# Build for RO on RO
GCCSDK_INSTALL_ENV := <NSLibs$$Dir>
CC := gcc
EXEEXT :=
PKG_CONFIG :=
else
# Cross-build for RO (either using GCCSDK 3.4.6 - AOF,
# either using GCCSDK 4 - ELF)
GCCSDK_INSTALL_ENV ?= /home/riscos/env
GCCSDK_INSTALL_CROSSBIN ?= /home/riscos/cross/bin
CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)
ifneq (,$(findstring arm-unknown-riscos-gcc,$(CC)))
SUBTARGET := -elf
EXEEXT := ,e1f
ELF2AIF := $(GCCSDK_INSTALL_CROSSBIN)/elf2aif
else
SUBTARGET := -aof
EXEEXT := ,ff8
endif
PKG_CONFIG := $(GCCSDK_INSTALL_ENV)/ro-pkg-config
CCACHE := $(shell which ccache)
ifneq ($(CCACHE),)
CC := $(CCACHE) $(CC)
endif
endif
else
ifeq ($(TARGET),beos)
# Building for BeOS/Haiku
#ifeq ($(HOST),beos)
# Build for BeOS on BeOS
GCCSDK_INSTALL_ENV := /boot/develop
CC := gcc
CXX := g++
EXEEXT :=
PKG_CONFIG :=
#endif
else
# Building for GTK or debug
PKG_CONFIG := pkg-config
endif
endif

OBJROOT := build-$(HOST)-$(TARGET)$(SUBTARGET)

# ----------------------------------------------------------------------------
# General flag setup
# ----------------------------------------------------------------------------

include Makefile.config

# 1: Feature name (ie, NETSURF_USE_BMP -> BMP)
# 2: Parameters to add to CFLAGS
# 3: Parameters to add to LDFLAGS
define feature_enabled
 ifeq ($$(NETSURF_USE_$(1)),YES)
   CFLAGS += $(2)
   LDFLAGS += $(3)
 endif
endef

$(eval $(call feature_enabled,BMP,-DWITH_BMP,))
$(eval $(call feature_enabled,GIF,-DWITH_GIF,))
$(eval $(call feature_enabled,JPEG,-DWITH_JPEG,-ljpeg))
$(eval $(call feature_enabled,MNG,-DWITH_MNG,-lmng))

$(eval $(call feature_enabled,HARU_PDF,-DWITH_PDF_EXPORT,-lhpdf -lpng))

# common libraries without pkg-config support
LDFLAGS += -lz

# ----------------------------------------------------------------------------
# RISC OS host flag setup
# ----------------------------------------------------------------------------

ifeq ($(TARGET),riscos)
ifeq ($(HOST),riscos)
LDFLAGS += -Xlinker -symbols=$(OBJROOT)/sym -lxml2 -lz -lm -lcurl -lssl \
	-lcrypto -lcares
else
LDFLAGS += $(shell $(PKG_CONFIG) --libs libxml-2.0 libcurl openssl)
endif

$(eval $(call feature_enabled,NSSVG,-DWITH_NS_SVG,-lsvgtiny))
$(eval $(call feature_enabled,DRAW,-DWITH_DRAW,-lpencil))
$(eval $(call feature_enabled,SPRITE,-DWITH_SPRITE,))
$(eval $(call feature_enabled,ARTWORKS,-DWITH_ARTWORKS,))

endif

# ----------------------------------------------------------------------------
# BeOS flag setup
# ----------------------------------------------------------------------------

ifeq ($(HOST),beos)
LDFLAGS += -L/boot/home/config/lib
LDFLAGS += -lxml2 -lz -lcurl -lssl -lcrypto -liconv
endif

# ----------------------------------------------------------------------------
# GTK flag setup (using pkg-config)
# ----------------------------------------------------------------------------

ifeq ($(TARGET),gtk)
LDFLAGS += $(shell $(PKG_CONFIG) --libs libxml-2.0 libcurl openssl)

# 1: Feature name (ie, NETSURF_USE_RSVG -> RSVG)
# 2: pkg-config required modules for feature
# 3: Human-readable name for the feature
define pkg_config_find_and_add

 ifneq ($$(NETSURF_USE_$(1)),NO)
  NETSURF_FEATURE_$(1)_AVAILABLE := $$(shell pkg-config --exists $(2) && echo yes)
  ifeq ($$(NETSURF_USE_$(1)),AUTO)
   ifeq ($$(NETSURF_FEATURE_$(1)_AVAILABLE),yes)
    NETSURF_USE_$(1) := YES
   endif
  endif
  ifeq ($$(NETSURF_USE_$(1)),YES)
   ifeq ($$(NETSURF_FEATURE_$(1)_AVAILABLE),yes)
    CFLAGS += $$(shell pkg-config --cflags $(2)) $$(NETSURF_FEATURE_$(1)_CFLAGS)
    LDFLAGS += $$(shell pkg-config --libs $(2)) $$(NETSURF_FEATURE_$(1)_LDFLAGS)
    $$(info AUTOCONF: auto-enabled $(3) ($(2)).)
   else
    $$(error Unable to find library for: $$(3) ($(2))
   endif
  endif
 endif

endef

# define additional CFLAGS and LDFLAGS requirements for pkg-configed libs here
NETSURF_FEATURE_RSVG_CFLAGS := -DWITH_RSVG
NETSURF_FEATURE_ROSPRITE_CFLAGS := -DWITH_NSSPRITE

# add a line similar to below for each optional pkg-configed lib here
$(eval $(call pkg_config_find_and_add,RSVG,librsvg-2.0,SVG rendering))
$(eval $(call pkg_config_find_and_add,ROSPRITE,librosprite,RISC OS sprite rendering))

GTKCFLAGS := -std=c99 -Dgtk -Dnsgtk \
	-DGTK_DISABLE_DEPRECATED \
	-D_BSD_SOURCE \
	-D_XOPEN_SOURCE=600 \
	-D_POSIX_C_SOURCE=200112L \
	-D_NETBSD_SOURCE \
	-DGTK_RESPATH=\"$(NETSURF_GTK_RESOURCES)\" \
	$(WARNFLAGS) -I. -g $(OPT2FLAGS) \
	$(shell $(PKG_CONFIG) --cflags libglade-2.0 gtk+-2.0) \
	$(shell xml2-config --cflags)

GTKLDFLAGS := $(shell $(PKG_CONFIG) --cflags --libs libglade-2.0 gtk+-2.0 gthread-2.0 gmodule-2.0 lcms)

CFLAGS += $(GTKCFLAGS)
LDFLAGS += $(GTKLDFLAGS)

# ----------------------------------------------------------------------------
# Windows flag setup
# ----------------------------------------------------------------------------

ifeq ($(HOST),Windows_NT)
CFLAGS += -U__STRICT_ANSI__
endif

endif

# ----------------------------------------------------------------------------
# RISC OS target flag setup
# ----------------------------------------------------------------------------

ifeq ($(TARGET),riscos)
CFLAGS += -I. $(OPTFLAGS) $(WARNFLAGS) -Driscos		\
	-std=c99 -D_BSD_SOURCE -D_POSIX_C_SOURCE	\
	-mpoke-function-name

CFLAGS += -I$(GCCSDK_INSTALL_ENV)/include		\
	-I$(GCCSDK_INSTALL_ENV)/include/libxml2		\
	-I$(GCCSDK_INSTALL_ENV)/include/libmng
ifeq ($(HOST),riscos)
CFLAGS += -I<OSLib$$Dir> -mthrowback
endif
ASFLAGS += -xassembler-with-cpp -I. -I$(GCCSDK_INSTALL_ENV)/include
LDFLAGS += -L$(GCCSDK_INSTALL_ENV)/lib -lrufl
ifeq ($(HOST),riscos)
LDFLAGS += -LOSLib: -lOSLib32
else
LDFLAGS += -lOSLib32
ifeq ($(SUBTARGET),-elf)
# Go for static builds & AIF binary at the moment:
CFLAGS += -static
LDFLAGS += -static
EXEEXT := ,ff8
endif
endif
endif

# ----------------------------------------------------------------------------
# BeOS target flag setup
# ----------------------------------------------------------------------------

ifeq ($(TARGET),beos)
CFLAGS += -I. -O $(WARNFLAGS) -Dnsbeos			\
	-D_BSD_SOURCE -D_POSIX_C_SOURCE			\
	-Drestrict="" -Wno-multichar 
# DEBUG
CFLAGS += -g -O0
# -DDEBUG=1

BEOS_BERES := beres
BEOS_RC := rc
BEOS_XRES := xres
BEOS_SETVER := setversion
BEOS_MIMESET := mimeset
VERSION_FULL := $(shell sed -n '/"/{s/.*"\(.*\)".*/\1/;p;}' desktop/version.c)
VERSION_MAJ := $(shell sed -n '/_major/{s/.* = \([0-9]*\).*/\1/;p;}' desktop/version.c)
VERSION_MIN := $(shell sed -n '/_minor/{s/.* = \([0-9]*\).*/\1/;p;}' desktop/version.c)
RSRC_BEOS = $(addprefix $(OBJROOT)/,$(subst /,_,$(patsubst %.rdef,%.rsrc,$(RDEF_BEOS))))
RESOURCES = $(RSRC_BEOS)
ifeq ($(HOST),beos)
CFLAGS += -I/boot/home/config/include		\
	-I/boot/home/config/include/libxml2		\
	-I/boot/home/config/include/libmng
ifneq ($(wildcard /boot/develop/lib/*/libzeta.so),)
LDFLAGS += -lzeta
endif
ifneq ($(wildcard /boot/develop/lib/*/libnetwork.so),)
# Haiku
NETLDFLAGS := -lnetwork
else
ifneq ($(wildcard /boot/develop/lib/*/libbind.so),)
# BONE
NETLDFLAGS := -lsocket -lbind
else
# net_server, will probably never work
NETLDFLAGS := -lnet
endif
endif
else
# cross: Haiku ?
NETLDFLAGS := -lnetwork
endif
LDFLAGS += -lbe -ltranslation $(NETLDFLAGS)
endif

# ----------------------------------------------------------------------------
# Debug target setup
# ----------------------------------------------------------------------------

ifeq ($(TARGET),debug)
CFLAGS += -std=c99 -DDEBUG_BUILD \
	-D_BSD_SOURCE \
	-D_XOPEN_SOURCE=600 \
	-D_POSIX_C_SOURCE=200112L \
	-D_NETBSD_SOURCE \
	$(WARNFLAGS) -I. -I../../libsprite/trunk/ -g $(OPT0FLAGS) \
	$(shell $(PKG_CONFIG) --cflags librosprite) \
	$(shell xml2-config --cflags)
LDFLAGS += $(shell $(PKG_CONFIG) --libs librosprite)
endif

# ----------------------------------------------------------------------------
# General make rules
# ----------------------------------------------------------------------------

$(OBJROOT)/created:
	$(VQ)echo "   MKDIR: $(OBJROOT)"
	$(Q)$(MKDIR) $(OBJROOT)
	$(Q)$(TOUCH) $(OBJROOT)/created

DEPROOT := $(OBJROOT)/deps
$(DEPROOT)/created: $(OBJROOT)/created
	$(VQ)echo "   MKDIR: $(DEPROOT)"
	$(Q)$(MKDIR) $(DEPROOT)
	$(Q)$(TOUCH) $(DEPROOT)/created

WARNFLAGS = -W -Wall -Wundef -Wpointer-arith \
	-Wcast-align -Wwrite-strings -Wstrict-prototypes \
	-Wmissing-prototypes -Wmissing-declarations -Wredundant-decls \
	-Wnested-externs -Winline 
ifneq ($(GCCVER),2)
WARNFLAGS += -Wno-unused-parameter 
endif

OPT0FLAGS = -O0
# -O and -O2 can use -Wuninitialized which gives us more static checking.
# unfortunately the optimiser is what provides the hints in the code tree
# so we cannot do it when we do -O0 (E.g. debug)
OPTFLAGS = -O -Wuninitialized
OPT2FLAGS = -O2 -Wuninitialized

CLEANS := clean-target

include Makefile.sources

OBJECTS := $(sort $(addprefix $(OBJROOT)/,$(subst /,_,$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(patsubst %.s,%.o,$(SOURCES)))))))

$(EXETARGET): $(OBJECTS) $(RESOURCES)
	$(VQ)echo "    LINK: $(EXETARGET)"
ifneq ($(TARGET)$(SUBTARGET),riscos-elf)
	$(Q)$(CC) -o $(EXETARGET) $(OBJECTS) $(LDFLAGS)
else
	$(Q)$(CC) -o $(EXETARGET:,ff8=,e1f) $(OBJECTS) $(LDFLAGS)
	$(Q)$(ELF2AIF) $(EXETARGET:,ff8=,e1f) $(EXETARGET)
	$(Q)$(RM) $(EXETARGET:,ff8=,e1f)
endif
ifeq ($(TARGET),beos)
	$(VQ)echo "    XRES: $(EXETARGET)"
	$(Q)$(BEOS_XRES) -o $(EXETARGET) $(RSRC_BEOS)
	$(VQ)echo "  SETVER: $(EXETARGET)"
	$(Q)$(BEOS_SETVER) $(EXETARGET) \
                -app $(VERSION_MAJ) $(VERSION_MIN) 0 d 0 \
                -short "NetSurf $(VERSION_FULL)" \
                -long "NetSurf $(VERSION_FULL) ©"
	$(VQ)echo " MIMESET: $(EXETARGET)"
	$(Q)$(BEOS_MIMESET) $(EXETARGET)
endif

ifeq ($(TARGET),beos)
$(RSRC_BEOS): $(RDEF_BEOS)
	$(VQ)echo "      RC: $<"
	$(Q)$(BEOS_RC) -o $@ $<
endif

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
ifeq ($(GCCVER),2)
# simpler deps tracking for gcc2...
define dependency_generate_c
DEPFILES += $(2)
$$(DEPROOT)/$(2): $$(DEPROOT)/created $(1) css/css_enum.h css/parser.h Makefile.config
	$$(VQ)echo "     DEP: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(2)
	$$(Q)$$(CC) $$(CFLAGS) -MM  \
		    $(1) | sed 's,^.*:,$$(DEPROOT)/$2 $$(OBJROOT)/$(3):,' \
		    > $$(DEPROOT)/$(2)

endef
else
define dependency_generate_c
DEPFILES += $(2)
$$(DEPROOT)/$(2): $$(DEPROOT)/created $(1) css/css_enum.h css/parser.h Makefile.config
	$$(VQ)echo "     DEP: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(2)
	$$(Q)$$(CC) $$(CFLAGS) -MM -MT '$$(DEPROOT)/$2 $$(OBJROOT)/$(3)' \
		    -MF $$(DEPROOT)/$(2) $(1)

endef
endif

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

define compile_target_cpp
$$(OBJROOT)/$(2): $$(OBJROOT)/created $$(DEPROOT)/$(3)
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(CXX) $$(CFLAGS) -o $$@ -c $(1)

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

$(eval $(foreach SOURCE,$(filter %.cpp,$(SOURCES)), \
	$(call dependency_generate_c,$(SOURCE),$(subst /,_,$(SOURCE:.cpp=.d)),$(subst /,_,$(SOURCE:.cpp=.o)))))

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

$(eval $(foreach SOURCE,$(filter %.cpp,$(SOURCES)), \
	$(call compile_target_cpp,$(SOURCE),$(subst /,_,$(SOURCE:.cpp=.o)),$(subst /,_,$(SOURCE:.cpp=.d)))))

$(eval $(foreach SOURCE,$(filter %.s,$(SOURCES)), \
	$(call compile_target_s,$(SOURCE),$(subst /,_,$(SOURCE:.s=.o)),$(subst /,_,$(SOURCE:.s=.d)))))

.PHONY: all clean docs install install-gtk

clean: $(CLEANS)

install-gtk:
	mkdir -p $(DESTDIR)/usr/share/netsurf/throbber
	mkdir -p $(DESTDIR)/usr/bin
	@cp -v nsgtk $(DESTDIR)/usr/bin
	@cp -vrL gtk/res/adblock.css $(DESTDIR)/usr/share/netsurf
	@cp -vrL gtk/res/ca-bundle.txt $(DESTDIR)/usr/share/netsurf
	@cp -vrL gtk/res/default.css $(DESTDIR)/usr/share/netsurf
	@cp -vrL gtk/res/gtkdefault.css $(DESTDIR)/usr/share/netsurf
	@cp -vrL gtk/res/license $(DESTDIR)/usr/share/netsurf
	@cp -vrL gtk/res/netsurf.xpm $(DESTDIR)/usr/share/netsurf
	@cp -vrL gtk/res/netsurf-16x16.xpm $(DESTDIR)/usr/share/netsurf
	@cp -vrL gtk/res/throbber/*.png $(DESTDIR)/usr/share/netsurf/throbber
	gzip -9v < gtk/res/messages > $(DESTDIR)/usr/share/netsurf/messages
	gzip -9v < gtk/res/downloads.glade > $(DESTDIR)/usr/share/netsurf/downloads.glade
	gzip -9v < gtk/res/netsurf.glade > $(DESTDIR)/usr/share/netsurf/netsurf.glade
	gzip -9v < gtk/res/options.glade > $(DESTDIR)/usr/share/netsurf/options.glade

install: all-program install-$(TARGET)

docs:
	doxygen Docs/Doxyfile
