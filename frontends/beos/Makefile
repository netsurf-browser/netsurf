# ----------------------------------------------------------------------------
# BeOS target setup
# ----------------------------------------------------------------------------

# Beos build tools
BEOS_BERES := beres
BEOS_RC := rc
BEOS_XRES := xres
BEOS_SETVER := setversion
BEOS_MIMESET := mimeset

# Linker flags
LDFLAGS += -L/boot/home/config/lib
LDFLAGS += -L/boot/common/lib
LDFLAGS += -lbe -ltranslation -ltracker -lcolumnlistview -lnetwork
ifeq ($(CC_MAJOR),2)
    LDFLAGS += -lstdc++.r4
else
    LDFLAGS += -lstdc++ -lsupc++
endif
LDFLAGS += -L$(PREFIX)/lib

COMMON_WARNFLAGS += -Wno-multichar

# compiler flags
CFLAGS += -std=c99 -Dnsbeos -D_BSD_SOURCE -D_POSIX_C_SOURCE -Drestrict="" -g
CFLAGS += -I$(PREFIX)/include

CXXFLAGS += -Dnsbeos -D_BSD_SOURCE -D_POSIX_C_SOURCE -Drestrict="" -g
CXXFLAGS += -I$(PREFIX)/include

VERSION_FULL := $(shell sed -n '/_version.*=.*"/{s/.*"\(.*\)".*/\1/;p;}' desktop/version.c)
VERSION_MAJ := $(shell sed -n '/_major/{s/.* = \([0-9]*\).*/\1/;p;}' desktop/version.c)
VERSION_MIN := $(shell sed -n '/_minor/{s/.* = \([0-9]*\).*/\1/;p;}' desktop/version.c)

# ----------------------------------------------------------------------------
# Source file setup
# ----------------------------------------------------------------------------

# sources purely for the BeOS build
S_FRONTEND := about.cpp bitmap.cpp cookies.cpp download.cpp		\
	fetch_rsrc.cpp filetype.cpp font.cpp gui.cpp login.cpp		\
	gui_options.cpp plotters.cpp scaffolding.cpp search.cpp		\
	schedule.cpp throbber.cpp window.cpp

# This is the final source build list
# Note this is deliberately *not* expanded here as common and image
#   are not yet available
SOURCES = $(S_COMMON) $(S_IMAGE) $(S_BROWSER) $(S_FRONTEND)
EXETARGET := NetSurf

# The filter and target for split messages
MESSAGES_FILTER=beos
MESSAGES_TARGET=$(FRONTEND_RESOURCES_DIR)

# ----------------------------------------------------------------------------
# Resources
# ----------------------------------------------------------------------------

RDEF_BEOS := res.rdef
RDEF_BEOS := $(addprefix $(FRONTEND_SOURCE_DIR)/,$(RDEF_BEOS))

RDEF_IMP_BEOS := res_import.rdef
RDEF_IMP_BEOS := $(addprefix $(OBJROOT)/,$(subst /,_,$(RDEF_IMP_BEOS)))

RDEP_BEOS := \
	adblock.css beosdefault.css default.css internal.css quirks.css	\
	netsurf.png favicon.png ca-bundle.txt \
	credits.html licence.html welcome.html maps.html SearchEngines

RDEP_BEOS := $(addprefix $(FRONTEND_RESOURCES_DIR)/,$(RDEP_BEOS))	\
	$(wildcard $(FRONTEND_RESOURCES_DIR)/icons/*.png)		\
	$(wildcard $(FRONTEND_RESOURCES_DIR)/??/*)			\
	$(wildcard $(FRONTEND_RESOURCES_DIR)/throbber/throbber*.png)

RSRC_BEOS = $(addprefix $(OBJROOT)/,$(subst /,_,$(patsubst %.rdef,%.rsrc,$(RDEF_BEOS))))
RESOURCES = $(RSRC_BEOS)

$(RDEF_IMP_BEOS): $(RDEP_BEOS)
	$(VQ)echo "     GEN: $@"
	$(Q)n=5000; for f in $^; do echo "resource($$n,\"$${f#$(FRONTEND_RESOURCES_DIR)/}\") #'data' import \"$${f#$(FRONTEND_SOURCE_DIR)/}\";"; n=$$(($$n+1)); done > $@

$(RSRC_BEOS): $(RDEF_BEOS) $(RDEF_IMP_BEOS)
	$(VQ)echo "      RC: $<"
	$(Q)$(BEOS_RC) -I $(FRONTEND_SOURCE_DIR) -o $@ $^


# ----------------------------------------------------------------------------
# Install target
# ----------------------------------------------------------------------------

install-beos:
	mkdir -p $(DESTDIR)$(NETSURF_BEOS_BIN)
	mkdir -p $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@copyattr -d $(EXETARGET) $(DESTDIR)$(NETSURF_BEOS_BIN)NetSurf
	@cp -vRL $(FRONTEND_RESOURCES_DIR)/adblock.css $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@cp -vRL $(FRONTEND_RESOURCES_DIR)/ca-bundle.txt $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@cp -vRL $(FRONTEND_RESOURCES_DIR)/default.css $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@cp -vRL $(FRONTEND_RESOURCES_DIR)/beosdefault.css $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@cp -vRL $(FRONTEND_RESOURCES_DIR)/license $(DESTDIR)$(NETSURF_BEOS_RESOURCES)
	@cp -vRL $(FRONTEND_RESOURCES_DIR)/SearchEngines $(DESTDIR)$(NETSURF_BEOS_RESOURCES)

# ----------------------------------------------------------------------------
# Package target
# ----------------------------------------------------------------------------

package-beos:
