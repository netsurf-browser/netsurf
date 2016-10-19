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

.PHONY: all

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
#	have an up-to-date RISC OS build system. ;-)
HOST := $(shell uname -s)

# Sanitise host
# TODO: Ideally, we want the equivalent of s/[^A-Za-z0-9]/_/g here
HOST := $(subst .,_,$(subst -,_,$(subst /,_,$(HOST))))


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
endif

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
    ifeq ($(TARGET),haiku)
      TARGET := beos
    endif
endif

ifeq ($(HOST),AmigaOS)
  HOST := amiga
  ifeq ($(TARGET),)
    TARGET := amiga
  endif
endif

ifeq ($(HOST),Darwin)
  HOST := macosx
  ifeq ($(TARGET),)
    TARGET := cocoa
  endif
endif

ifeq ($(HOST),FreeMiNT)
  HOST := mint
endif
ifeq ($(HOST),mint)
  ifeq ($(TARGET),)
    TARGET := atari
  endif
endif

ifeq ($(findstring MINGW,$(HOST)),MINGW)
  # MSYS' uname reports the likes of "MINGW32_NT-6.0"
  HOST := windows
endif
ifeq ($(HOST),windows)
  ifeq ($(TARGET),)
    TARGET := windows
  endif
endif

# Default target is GTK backend
ifeq ($(TARGET),)
  TARGET := gtk
endif

# valid values for the TARGET
VLDTARGET := riscos gtk gtk3 beos amiga amigaos3 framebuffer windows atari cocoa monkey

# Check for valid TARGET
ifeq ($(filter $(VLDTARGET),$(TARGET)),)
  $(error Unknown TARGET "$(TARGET)", Must be one of $(VLDTARGET))
endif

# ensure empty values for base variables

# Sub target for build
SUBTARGET=
# Resources executable target depends upon
RESOURCES=
# Messages executable target depends on
MESSAGES:=

# The filter applied to the fat (full) messages to generate split messages
MESSAGES_FILTER=any
# The languages in the fat messages to convert
MESSAGES_LANGUAGES=de en fr it nl
# The target directory for the split messages
MESSAGES_TARGET=!NetSurf/Resources

# Defaults for tools
PERL=perl
MKDIR=mkdir
TOUCH=touch
STRIP?=strip
SPLIT_MESSAGES=$(PERL) utils/split-messages.pl

# build verbosity
ifeq ($(V),1)
  Q:=
else
  Q=@
endif
VQ=@

# Override this only if the host compiler is called something different
HOST_CC := gcc

ifeq ($(TARGET),riscos)
  ifeq ($(HOST),riscos)
    # Build for RO on RO
    GCCSDK_INSTALL_ENV := <NSLibs$$Dir>
    CCRES := ccres
    TPLEXT :=
    MAKERUN := makerun
    SQUEEZE := squeeze
    RUNEXT :=
    CC := gcc
    CXX := g++
    EXEEXT :=
    PKG_CONFIG :=
  else
    # Cross-build for RO (either using GCCSDK 3.4.6 - AOF,
    # either using GCCSDK 4 - ELF)
    ifeq ($(origin GCCSDK_INSTALL_ENV),undefined)
      ifneq ($(realpath /opt/netsurf/arm-unknown-riscos/env),)
        GCCSDK_INSTALL_ENV := /opt/netsurf/arm-unknown-riscos/env
      else
        GCCSDK_INSTALL_ENV := /home/riscos/env
      endif
    endif

    ifeq ($(origin GCCSDK_INSTALL_CROSSBIN),undefined)
      ifneq ($(realpath /opt/netsurf/arm-unknown-riscos/cross/bin),)
        GCCSDK_INSTALL_CROSSBIN := /opt/netsurf/arm-unknown-riscos/cross/bin
      else
        GCCSDK_INSTALL_CROSSBIN := /home/riscos/cross/bin
      endif
    endif

    CCRES := $(GCCSDK_INSTALL_CROSSBIN)/ccres
    TPLEXT := ,fec
    MAKERUN := $(GCCSDK_INSTALL_CROSSBIN)/makerun
    SQUEEZE := $(GCCSDK_INSTALL_CROSSBIN)/squeeze
    RUNEXT := ,feb
    CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)
    ifneq (,$(findstring arm-unknown-riscos-gcc,$(CC)))
      SUBTARGET := -elf
      EXEEXT := ,e1f
      ELF2AIF := $(GCCSDK_INSTALL_CROSSBIN)/elf2aif
    else
     SUBTARGET := -aof
     EXEEXT := ,ff8
    endif
    CXX := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*g++)
    PKG_CONFIG := $(GCCSDK_INSTALL_ENV)/ro-pkg-config
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
      PKG_CONFIG := pkg-config
    #endif
  else
    ifeq ($(TARGET),windows)
      ifneq ($(HOST),windows)
        # Set Mingw defaults
        GCCSDK_INSTALL_ENV ?= /opt/netsurf/i686-w64-mingw32/env
        GCCSDK_INSTALL_CROSSBIN ?= /opt/netsurf/i686-w64-mingw32/cross/bin

        CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)
        WINDRES := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*windres)

        PKG_CONFIG := PKG_CONFIG_LIBDIR="$(GCCSDK_INSTALL_ENV)/lib/pkgconfig" pkg-config
      else
        # Building on Windows
        CC := gcc
        PKG_CONFIG :=
      endif
    else
      ifeq ($(findstring amiga,$(TARGET)),amiga)
        ifeq ($(findstring amiga,$(HOST)),amiga)
          PKG_CONFIG := pkg-config
        else
          ifeq ($(TARGET),amigaos3)
            GCCSDK_INSTALL_ENV ?= /opt/netsurf/m68k-unknown-amigaos/env
            GCCSDK_INSTALL_CROSSBIN ?= /opt/netsurf/m68k-unknown-amigaos/cross/bin

            SUBTARGET = os3
          else
            GCCSDK_INSTALL_ENV ?= /opt/netsurf/ppc-amigaos/env
            GCCSDK_INSTALL_CROSSBIN ?= /opt/netsurf/ppc-amigaos/cross/bin
          endif

          override TARGET := amiga

          CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)

          PKG_CONFIG := PKG_CONFIG_LIBDIR="$(GCCSDK_INSTALL_ENV)/lib/pkgconfig" pkg-config
        endif
      else
        ifeq ($(TARGET),cocoa)
          PKG_CONFIG := PKG_CONFIG_PATH="$(PKG_CONFIG_PATH):/usr/local/lib/pkgconfig" pkg-config
        else
          ifeq ($(TARGET),atari)
            ifeq ($(HOST),atari)
              PKG_CONFIG := pkg-config
            else
              ifeq ($(HOST),mint)
                PKG_CONFIG := pkg-config
              else
                GCCSDK_INSTALL_ENV ?= /opt/netsurf/m68k-atari-mint/env
                GCCSDK_INSTALL_CROSSBIN ?= /opt/netsurf/m68k-atari-mint/cross/bin

                CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)

                PKG_CONFIG := PKG_CONFIG_LIBDIR="$(GCCSDK_INSTALL_ENV)/lib/pkgconfig" pkg-config
              endif
            endif
          else
	    ifeq ($(TARGET),monkey)
              ifeq ($(origin GCCSDK_INSTALL_ENV),undefined)
                PKG_CONFIG := pkg-config
              else
                PKG_CONFIG := PKG_CONFIG_LIBDIR="$(GCCSDK_INSTALL_ENV)/lib/pkgconfig" pkg-config                
              endif

              ifneq ($(origin GCCSDK_INSTALL_CROSSBIN),undefined)
                CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)
                CXX := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*g++)
              endif
	    else
	      ifeq ($(TARGET),framebuffer)
                ifeq ($(origin GCCSDK_INSTALL_ENV),undefined)
                  PKG_CONFIG := pkg-config
                else
                  PKG_CONFIG := PKG_CONFIG_LIBDIR="$(GCCSDK_INSTALL_ENV)/lib/pkgconfig" pkg-config                
                endif

                ifneq ($(origin GCCSDK_INSTALL_CROSSBIN),undefined)
                  CC := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*gcc)
                  CXX := $(wildcard $(GCCSDK_INSTALL_CROSSBIN)/*g++)
                endif
              else
                # All native targets
                PKG_CONFIG := pkg-config

                # gtk target processing
	        ifeq ($(TARGET),gtk3)
                  override TARGET := gtk
                  override NETSURF_GTK_MAJOR := 3
                  SUBTARGET = $(NETSURF_GTK_MAJOR)
                else
	          ifeq ($(TARGET),gtk)
                    ifeq ($(origin NETSURF_GTK_MAJOR),undefined)
                      override NETSURF_GTK_MAJOR := 2
                    else
                      ifneq ($(NETSURF_GTK_MAJOR),2)
                        SUBTARGET = $(NETSURF_GTK_MAJOR)
                      endif
                    endif
                  endif
                endif
              endif
            endif
          endif
        endif
      endif
    endif
  endif
endif

# compiler versioning to adjust warning flags
CC_VERSION := $(shell $(CC) -dumpversion)
CC_MAJOR := $(word 1,$(subst ., ,$(CC_VERSION)))
CC_MINOR := $(word 2,$(subst ., ,$(CC_VERSION)))
define cc_ver_ge
$(shell expr $(CC_MAJOR) \>= $(1) \& $(CC_MINOR) \>= $(2))
endef

# CCACHE
ifeq ($(origin CCACHE),undefined)
  CCACHE=$(word 1,$(shell ccache -V 2>/dev/null))
endif
CC := $(CCACHE) $(CC)

# Target paths
OBJROOT = build-$(HOST)-$(TARGET)$(SUBTARGET)
DEPROOT := $(OBJROOT)/deps
TOOLROOT := $(OBJROOT)/tools

# keep C flags from environment
CFLAGS_ENV := $(CFLAGS)
CXXFLAGS_ENV := $(CXXFLAGS)

# A macro that conditionaly adds flags to the build when a feature is enabled.
#
# 1: Feature name (ie, NETSURF_USE_BMP -> BMP)
# 2: Parameters to add to CFLAGS
# 3: Parameters to add to LDFLAGS
# 4: Human-readable name for the feature
define feature_enabled
  ifeq ($$(NETSURF_USE_$(1)),YES)
    CFLAGS += $(2)
    CXXFLAGS += $(2)
    LDFLAGS += $(3)
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(4)	enabled       (NETSURF_USE_$(1) := YES))
    endif
  else ifeq ($$(NETSURF_USE_$(1)),NO)
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(4)	disabled      (NETSURF_USE_$(1) := NO))
    endif
  else
    $$(info M.CONFIG: $(4)	error         (NETSURF_USE_$(1) := $$(NETSURF_USE_$(1))))
    $$(error NETSURF_USE_$(1) must be YES or NO)
  endif
endef

# A macro that conditionaly adds flags to the build with a uniform display.
#
# 1: Feature name (ie, NETSURF_USE_BMP -> BMP)
# 2: Human-readable name for the feature
# 3: Parameters to add to CFLAGS when enabled
# 4: Parameters to add to LDFLAGS when enabled
# 5: Parameters to add to CFLAGS when disabled
# 6: Parameters to add to LDFLAGS when disabled
define feature_switch
  ifeq ($$(NETSURF_USE_$(1)),YES)
    CFLAGS += $(3)
    CXXFLAGS += $(3)
    LDFLAGS += $(4)
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(2)	enabled       (NETSURF_USE_$(1) := YES))
    endif
  else ifeq ($$(NETSURF_USE_$(1)),NO)
    CFLAGS += $(5)
    CXXFLAGS += $(5)
    LDFLAGS += $(6)
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(2)	disabled      (NETSURF_USE_$(1) := NO))
    endif
  else
    $$(info M.CONFIG: $(4)	error         (NETSURF_USE_$(1) := $$(NETSURF_USE_$(1))))
    $$(error NETSURF_USE_$(1) must be YES or NO)
  endif
endef

# Extend flags with appropriate values from pkg-config for enabled features
#
# 1: pkg-config required modules for feature
# 2: Human-readable name for the feature
define pkg_config_find_and_add
  ifeq ($$(PKG_CONFIG),)
    $$(error pkg-config is required to auto-detect feature availability)
  endif

  PKG_CONFIG_$(1)_EXISTS := $$(shell $$(PKG_CONFIG) --exists $(1) && echo yes)

  ifeq ($$(PKG_CONFIG_$(1)_EXISTS),yes)
      CFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(1))
      CXXFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(1))
      LDFLAGS += $$(shell $$(PKG_CONFIG) --libs $(1))
      ifneq ($(MAKECMDGOALS),clean)
        $$(info PKG.CNFG: $(2) ($(1))	enabled)
      endif
  else
    ifneq ($(MAKECMDGOALS),clean)
      $$(info PKG.CNFG: $(2) ($(1))	failed)
      $$(error Unable to find library for: $(2) ($(1)))
    endif
  endif
endef

# Extend flags with appropriate values from pkg-config for enabled features
#
# 1: Feature name (ie, NETSURF_USE_RSVG -> RSVG)
# 2: pkg-config required modules for feature
# 3: Human-readable name for the feature
define pkg_config_find_and_add_enabled
  ifeq ($$(PKG_CONFIG),)
    $$(error pkg-config is required to auto-detect feature availability)
  endif

  NETSURF_FEATURE_$(1)_AVAILABLE := $$(shell $$(PKG_CONFIG) --exists $(2) && echo yes)

  ifeq ($$(NETSURF_USE_$(1)),YES)
    ifeq ($$(NETSURF_FEATURE_$(1)_AVAILABLE),yes)
      CFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(2)) $$(NETSURF_FEATURE_$(1)_CFLAGS)
      CXXFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(2)) $$(NETSURF_FEATURE_$(1)_CFLAGS)
      LDFLAGS += $$(shell $$(PKG_CONFIG) --libs $(2)) $$(NETSURF_FEATURE_$(1)_LDFLAGS)
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	enabled       (NETSURF_USE_$(1) := YES))
      endif
    else
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	failed        (NETSURF_USE_$(1) := YES))
        $$(error Unable to find library for: $(3) ($(2)))
      endif
    endif
  else ifeq ($$(NETSURF_USE_$(1)),AUTO)
    ifeq ($$(NETSURF_FEATURE_$(1)_AVAILABLE),yes)
      CFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(2)) $$(NETSURF_FEATURE_$(1)_CFLAGS)
      CXXFLAGS += $$(shell $$(PKG_CONFIG) --cflags $(2)) $$(NETSURF_FEATURE_$(1)_CFLAGS)
      LDFLAGS += $$(shell $$(PKG_CONFIG) --libs $(2)) $$(NETSURF_FEATURE_$(1)_LDFLAGS)
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	auto-enabled  (NETSURF_USE_$(1) := AUTO))
	NETSURF_USE_$(1) := YES
      endif
    else
      ifneq ($(MAKECMDGOALS),clean)
        $$(info M.CONFIG: $(3) ($(2))	auto-disabled (NETSURF_USE_$(1) := AUTO))
	NETSURF_USE_$(1) := NO
      endif
    endif
  else ifeq ($$(NETSURF_USE_$(1)),NO)
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(3) ($(2))	disabled      (NETSURF_USE_$(1) := NO))
    endif
  else
    ifneq ($(MAKECMDGOALS),clean)
      $$(info M.CONFIG: $(3) ($(2))	error         (NETSURF_USE_$(1) := $$(NETSURF_USE_$(1))))
      $$(error NETSURF_USE_$(1) must be YES, NO, or AUTO)
    endif
  endif
endef

# ----------------------------------------------------------------------------
# General flag setup
# ----------------------------------------------------------------------------

# Set up the warning flags here so that they can be overridden in the
#   Makefile.config
COMMON_WARNFLAGS = -W -Wall -Wundef -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wmissing-declarations -Wuninitialized

ifneq ($(CC_MAJOR),2)
  COMMON_WARNFLAGS += -Wno-unused-parameter
endif

# deal with lots of unwanted warnings from javascript
ifeq ($(call cc_ver_ge,4,6),1)
  COMMON_WARNFLAGS += -Wno-unused-but-set-variable
endif

# deal with chaging warning flags for different platforms
ifeq ($(HOST),OpenBSD)
  # OpenBSD headers are not compatible with redundant declaration warning
  COMMON_WARNFLAGS += -Wno-redundant-decls
else
  COMMON_WARNFLAGS += -Wredundant-decls
endif

# c++ default warning flags
CXXWARNFLAGS :=

# C default warning flags
CWARNFLAGS := -Wstrict-prototypes -Wmissing-prototypes -Wnested-externs

# Pull in the configuration
include Makefile.defaults

# libraries enabled by feature switch without pkgconfig file 
$(eval $(call feature_switch,JPEG,JPEG (libjpeg),-DWITH_JPEG,-ljpeg,-UWITH_JPEG,))
$(eval $(call feature_switch,HARU_PDF,PDF export (haru),-DWITH_PDF_EXPORT,-lhpdf -lpng,-UWITH_PDF_EXPORT,))
$(eval $(call feature_switch,LIBICONV_PLUG,glibc internal iconv,-DLIBICONV_PLUG,,-ULIBICONV_PLUG,-liconv))
$(eval $(call feature_switch,DUKTAPE,Javascript (Duktape),,,,,))

# Common libraries with pkgconfig
$(eval $(call pkg_config_find_and_add,libcss,CSS))
$(eval $(call pkg_config_find_and_add,libdom,DOM))
$(eval $(call pkg_config_find_and_add,libnsutils,nsutils))
$(eval $(call pkg_config_find_and_add,libutf8proc,utf8proc))

# Common libraries without pkg-config support
LDFLAGS += -lz

# Optional libraries with pkgconfig

# define additional CFLAGS and LDFLAGS requirements for pkg-configed libs
NETSURF_FEATURE_PNG_CFLAGS := -DWITH_PNG
NETSURF_FEATURE_BMP_CFLAGS := -DWITH_BMP
NETSURF_FEATURE_GIF_CFLAGS := -DWITH_GIF
NETSURF_FEATURE_CURL_CFLAGS := -DWITH_CURL
NETSURF_FEATURE_NSSVG_CFLAGS := -DWITH_NS_SVG
NETSURF_FEATURE_OPENSSL_CFLAGS := -DWITH_OPENSSL
NETSURF_FEATURE_ROSPRITE_CFLAGS := -DWITH_NSSPRITE
NETSURF_FEATURE_NSPSL_CFLAGS := -DWITH_NSPSL

# libcurl and openssl ordering matters as if libcurl requires ssl it
#  needs to come first in link order to ensure its symbols can be
#  resolved by the subsequent openssl

# freemint does not support pkg-config for libcurl
ifeq ($(HOST),mint)
    CFLAGS += $(shell curl-config --cflags)
    LDFLAGS += $(shell curl-config --libs)
else
    $(eval $(call pkg_config_find_and_add_enabled,CURL,libcurl,Curl))
endif
$(eval $(call pkg_config_find_and_add_enabled,OPENSSL,openssl,OpenSSL))

$(eval $(call pkg_config_find_and_add_enabled,PNG,libpng,PNG))
$(eval $(call pkg_config_find_and_add_enabled,BMP,libnsbmp,BMP))
$(eval $(call pkg_config_find_and_add_enabled,GIF,libnsgif,GIF))
$(eval $(call pkg_config_find_and_add_enabled,NSSVG,libsvgtiny,SVG))
$(eval $(call pkg_config_find_and_add_enabled,ROSPRITE,librosprite,Sprite))
$(eval $(call pkg_config_find_and_add_enabled,NSPSL,libnspsl,PSL))

# List of directories in which headers are searched for
INCLUDE_DIRS :=. include $(OBJROOT)

# export the user agent format
CFLAGS += -DNETSURF_UA_FORMAT_STRING=\"$(NETSURF_UA_FORMAT_STRING)\"
CXXFLAGS += -DNETSURF_UA_FORMAT_STRING=\"$(NETSURF_UA_FORMAT_STRING)\"

# set the default homepage to use
CFLAGS += -DNETSURF_HOMEPAGE=\"$(NETSURF_HOMEPAGE)\"
CXXFLAGS += -DNETSURF_HOMEPAGE=\"$(NETSURF_HOMEPAGE)\"

# ----------------------------------------------------------------------------
# General make rules
# ----------------------------------------------------------------------------

$(OBJROOT)/created:
	$(VQ)echo "   MKDIR: $(OBJROOT)"
	$(Q)$(MKDIR) -p $(OBJROOT)
	$(Q)$(TOUCH) $(OBJROOT)/created

$(DEPROOT)/created: $(OBJROOT)/created
	$(VQ)echo "   MKDIR: $(DEPROOT)"
	$(Q)$(MKDIR) -p $(DEPROOT)
	$(Q)$(TOUCH) $(DEPROOT)/created

$(TOOLROOT)/created: $(OBJROOT)/created
	$(VQ)echo "   MKDIR: $(TOOLROOT)"
	$(Q)$(MKDIR) -p $(TOOLROOT)
	$(Q)$(TOUCH) $(TOOLROOT)/created

CLEANS :=
POSTEXES :=

# ----------------------------------------------------------------------------
# Target specific setup
# ----------------------------------------------------------------------------

include frontends/Makefile

# ----------------------------------------------------------------------------
# General source file setup
# ----------------------------------------------------------------------------

# Content sources
include content/Makefile

# render sources
include render/Makefile

# utility sources
include utils/Makefile

# http utility sources
include utils/http/Makefile

# Desktop sources
include desktop/Makefile

# S_COMMON are sources common to all builds
S_COMMON := $(S_CONTENT) $(S_FETCHERS) $(S_RENDER) $(S_UTILS) $(S_HTTP) \
	$(S_DESKTOP) $(S_JAVASCRIPT_BINDING)


# ----------------------------------------------------------------------------
# Message targets
# ----------------------------------------------------------------------------

# Message splitting rule generation macro
# 1 = Language
define split_messages
.INTERMEDIATE:$$(MESSAGES_TARGET)/$(1)/Messages.tmp

$$(MESSAGES_TARGET)/$(1)/Messages.tmp: resources/FatMessages
	$$(VQ)echo "MSGSPLIT: Language: $(1) Filter: $$(MESSAGES_FILTER)"
	$$(Q)mkdir -p $$(MESSAGES_TARGET)/$(1)
	$$(Q)$$(SPLIT_MESSAGES) -l $(1) -p $$(MESSAGES_FILTER) -f messages -o $$@ $$<

$$(MESSAGES_TARGET)/$(1)/Messages: $$(MESSAGES_TARGET)/$(1)/Messages.tmp
	$$(VQ)echo "COMPRESS: $$@"
	$$(Q)gzip -9n < $$< > $$@

CLEAN_MESSAGES += $$(MESSAGES_TARGET)/$(1)/Messages
MESSAGES += $$(MESSAGES_TARGET)/$(1)/Messages

endef

# generate the message file rules
$(eval $(foreach LANG,$(MESSAGES_LANGUAGES), \
	$(call split_messages,$(LANG))))

clean-messages:
	$(VQ)echo "   CLEAN: $(CLEAN_MESSAGES)"
	$(Q)$(RM) $(CLEAN_MESSAGES)
CLEANS += clean-messages


# ----------------------------------------------------------------------------
# Source file setup
# ----------------------------------------------------------------------------

# Force exapnsion of source file list
SOURCES := $(SOURCES)

ifeq ($(SOURCES),)
$(error Unable to build NetSurf, could not determine set of sources to build)
endif

OBJECTS := $(sort $(addprefix $(OBJROOT)/,$(subst /,_,$(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(patsubst %.m,%.o,$(patsubst %.s,%.o,$(SOURCES))))))))

# Include directory flags
IFLAGS = $(addprefix -I,$(INCLUDE_DIRS))

$(EXETARGET): $(OBJECTS) $(RESOURCES) $(MESSAGES)
	$(VQ)echo "    LINK: $(EXETARGET)"
ifneq ($(TARGET)$(SUBTARGET),riscos-elf)
	$(Q)$(CC) -o $(EXETARGET) $(OBJECTS) $(LDFLAGS)
else
	$(Q)$(CXX) -o $(EXETARGET:,ff8=,e1f) $(OBJECTS) $(LDFLAGS)
	$(Q)$(ELF2AIF) $(EXETARGET:,ff8=,e1f) $(EXETARGET)
	$(Q)$(RM) $(EXETARGET:,ff8=,e1f)
endif
ifeq ($(NETSURF_STRIP_BINARY),YES)
	$(VQ)echo "   STRIP: $(EXETARGET)"
	$(Q)$(STRIP) $(EXETARGET)
endif
ifeq ($(TARGET),beos)
	$(VQ)echo "    XRES: $(EXETARGET)"
	$(Q)$(BEOS_XRES) -o $(EXETARGET) $(RSRC_BEOS)
	$(VQ)echo "  SETVER: $(EXETARGET)"
	$(Q)$(BEOS_SETVER) $(EXETARGET) \
                -app $(VERSION_MAJ) $(VERSION_MIN) 0 d 0 \
                -short "NetSurf $(VERSION_FULL)" \
                -long "NetSurf $(VERSION_FULL) © 2003 - 2016 The NetSurf Developers"
	$(VQ)echo " MIMESET: $(EXETARGET)"
	$(Q)$(BEOS_MIMESET) $(EXETARGET)
endif


clean-target:
	$(VQ)echo "   CLEAN: $(EXETARGET)"
	$(Q)$(RM) $(EXETARGET)
CLEANS += clean-target

clean-testament:
	$(VQ)echo "   CLEAN: testament.h"
	$(Q)$(RM) $(OBJROOT)/testament.h
CLEANS += clean-testament

clean-builddir:
	$(VQ)echo "   CLEAN: $(OBJROOT)"
	$(Q)$(RM) -r $(OBJROOT)
CLEANS += clean-builddir


.PHONY: all-program testament

testament $(OBJROOT)/testament.h:
	$(Q)$(PERL) utils/git-testament.pl $(CURDIR) $(OBJROOT)/testament.h

all-program: $(EXETARGET) $(POSTEXES)

.SUFFIXES:

DEPFILES :=
# Now some macros which build the make system

# 1 = Source file
# 2 = dep filename, no prefix
# 3 = obj filename, no prefix
define dependency_generate_c
DEPFILES += $(2)
$$(DEPROOT)/$(2): $$(DEPROOT)/created $(1) Makefile.config

endef

# 1 = Source file
# 2 = dep filename, no prefix
# 3 = obj filename, no prefix
define dependency_generate_s
DEPFILES += $(2)
$$(DEPROOT)/$(2): $$(DEPROOT)/created $(1)

endef

# 1 = Source file
# 2 = obj filename, no prefix
# 3 = dep filename, no prefix
ifeq ($(CC_MAJOR),2)
# simpler deps tracking for gcc2...
define compile_target_c
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo "     DEP: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(CC) $$(IFLAGS) $$(CFLAGS) -MM  \
		    $(1) | sed 's,^.*:,$$(DEPROOT)/$(3) $$(OBJROOT)/$(2):,' \
		    > $$(DEPROOT)/$(3)
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(RM) $$(OBJROOT)/$(2)
	$$(Q)$$(CC) $$(COMMON_WARNFLAGS) $$(CWARNFLAGS) $$(IFLAGS) $$(CFLAGS) $(CFLAGS_ENV) -o $$(OBJROOT)/$(2) -c $(1)

endef
else
define compile_target_c
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(RM) $$(OBJROOT)/$(2)
	$$(Q)$$(CC) $$(COMMON_WARNFLAGS) $$(CWARNFLAGS) $$(IFLAGS) $$(CFLAGS) $(CFLAGS_ENV) \
		    -MMD -MT '$$(DEPROOT)/$(3) $$(OBJROOT)/$(2)' \
		    -MF $$(DEPROOT)/$(3) -o $$(OBJROOT)/$(2) -c $(1)

endef
endif

define compile_target_cpp
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo "     DEP: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(CC) $$(IFLAGS) $$(CXXFLAGS) $$(COMMON_WARNFLAGS) $$(CXXWARNFLAGS) -MM  \
		    $(1) | sed 's,^.*:,$$(DEPROOT)/$(3) $$(OBJROOT)/$(2):,' \
		    > $$(DEPROOT)/$(3)
	$$(VQ)echo " COMPILE: $(1)"
	$$(Q)$$(RM) $$(OBJROOT)/$(2)
	$$(Q)$$(CXX) $$(COMMON_WARNFLAGS) $$(CXXWARNFLAGS) $$(IFLAGS) $$(CXXFLAGS) $(CXXFLAGS_ENV) -o $$(OBJROOT)/$(2) -c $(1)

endef

# 1 = Source file
# 2 = obj filename, no prefix
# 3 = dep filename, no prefix
define compile_target_s
$$(DEPROOT)/$(3) $$(OBJROOT)/$(2): $$(OBJROOT)/created
	$$(VQ)echo "ASSEMBLE: $(1)"
	$$(Q)$$(RM) $$(DEPROOT)/$(3)
	$$(Q)$$(RM) $$(OBJROOT)/$(2)
	$$(Q)$$(CC) $$(ASFLAGS) -MMD -MT '$$(DEPROOT)/$(3) $$(OBJROOT)/$(2)' \
		    -MF $$(DEPROOT)/$(3) -o $$(OBJROOT)/$(2) -c $(1)

endef

# Rules to construct dep lines for each object...
$(eval $(foreach SOURCE,$(filter %.c,$(SOURCES)), \
	$(call dependency_generate_c,$(SOURCE),$(subst /,_,$(SOURCE:.c=.d)),$(subst /,_,$(SOURCE:.c=.o)))))

$(eval $(foreach SOURCE,$(filter %.cpp,$(SOURCES)), \
	$(call dependency_generate_c,$(SOURCE),$(subst /,_,$(SOURCE:.cpp=.d)),$(subst /,_,$(SOURCE:.cpp=.o)))))

$(eval $(foreach SOURCE,$(filter %.m,$(SOURCES)), \
	$(call dependency_generate_c,$(SOURCE),$(subst /,_,$(SOURCE:.m=.d)),$(subst /,_,$(SOURCE:.m=.o)))))

# Cannot currently generate dep files for S files because they're objasm
# when we move to gas format, we will be able to.

#$(eval $(foreach SOURCE,$(filter %.s,$(SOURCES)), \
#	$(call dependency_generate_s,$(SOURCE),$(subst /,_,$(SOURCE:.s=.d)),$(subst /,_,$(SOURCE:.s=.o)))))

ifeq ($(filter $(MAKECMDGOALS),clean test coverage),)
-include $(sort $(addprefix $(DEPROOT)/,$(DEPFILES)))
endif

# And rules to build the objects themselves...

$(eval $(foreach SOURCE,$(filter %.c,$(SOURCES)), \
	$(call compile_target_c,$(SOURCE),$(subst /,_,$(SOURCE:.c=.o)),$(subst /,_,$(SOURCE:.c=.d)))))

$(eval $(foreach SOURCE,$(filter %.cpp,$(SOURCES)), \
	$(call compile_target_cpp,$(SOURCE),$(subst /,_,$(SOURCE:.cpp=.o)),$(subst /,_,$(SOURCE:.cpp=.d)))))

$(eval $(foreach SOURCE,$(filter %.m,$(SOURCES)), \
	$(call compile_target_c,$(SOURCE),$(subst /,_,$(SOURCE:.m=.o)),$(subst /,_,$(SOURCE:.m=.d)))))

$(eval $(foreach SOURCE,$(filter %.s,$(SOURCES)), \
	$(call compile_target_s,$(SOURCE),$(subst /,_,$(SOURCE:.s=.o)),$(subst /,_,$(SOURCE:.s=.d)))))

# ----------------------------------------------------------------------------
# Test setup
# ----------------------------------------------------------------------------

include test/Makefile


# ----------------------------------------------------------------------------
# Clean setup
# ----------------------------------------------------------------------------

.PHONY: clean

clean: $(CLEANS)


# ----------------------------------------------------------------------------
# build distribution package
# ----------------------------------------------------------------------------

.PHONY: package-$(TARGET) package

package: all-program package-$(TARGET)


# ----------------------------------------------------------------------------
# local install on the host system
# ----------------------------------------------------------------------------

.PHONY: install install-$(TARGET)

install: all-program install-$(TARGET)


# ----------------------------------------------------------------------------
# Documentation build
# ----------------------------------------------------------------------------

.PHONY: docs

docs: Docs/Doxyfile
	doxygen $<


# ----------------------------------------------------------------------------
# Transifex message processing
# ----------------------------------------------------------------------------

.PHONY: messages-split-tfx messages-fetch-tfx messages-import-tfx

# split fat messages into properties files suitable for uploading to transifex
messages-split-tfx:
	for splitlang in $(FAT_LANGUAGES);do perl ./utils/split-messages.pl -l $${splitlang} -f transifex -p any -o Messages.any.$${splitlang}.tfx resources/FatMessages;done

# download property files from transifex
messages-fetch-tfx:
	for splitlang in $(FAT_LANGUAGES);do $(RM) Messages.any.$${splitlang}.tfx ; perl ./utils/fetch-transifex.pl -w insecure -l $${splitlang} -o Messages.any.$${splitlang}.tfx ;done

# merge property files into fat messages
messages-import-tfx: messages-fetch-tfx
	for tfxlang in $(FAT_LANGUAGES);do perl ./utils/import-messages.pl -l $${tfxlang} -p any -f transifex -o resources/FatMessages -i resources/FatMessages -I Messages.any.$${tfxlang}.tfx ; $(RM) Messages.any.$${tfxlang}.tfx; done

