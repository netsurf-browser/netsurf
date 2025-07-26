#
# Makefile for NetSurf qt target
#
# This file is part of NetSurf
#
# ----------------------------------------------------------------------------
# QT flag setup (using pkg-config)
# ----------------------------------------------------------------------------

CFLAGS += -Dnsqt
CXXFLAGS += -DQT_RESPATH=\"$(NETSURF_QT_RES_PATH)\" -Dnsqt -g

$(eval $(call pkg_config_find_and_add,Qt6Widgets,Qt6Widgets))
$(eval $(call pkg_config_find_and_add,Qt6Core,Qt6Core))
$(eval $(call pkg_config_find_and_add,Qt6Gui,Qt6Gui))

LDFLAGS += -lm -lstdc++

# ---------------------------------------------------------------------------
# Target setup
# ---------------------------------------------------------------------------

# The binary target.
EXETARGET := nsqt

# The filter and target for split messages
MESSAGES_FILTER=qt
MESSAGES_TARGET=$(FRONTEND_RESOURCES_DIR)

# ----------------------------------------------------------------------------
# Builtin resource handling
# ----------------------------------------------------------------------------

QRC_FILE := netsurf

# use RCC to generate resource source
$(OBJROOT)/%.qrc.cpp: $(FRONTEND_RESOURCES_DIR)/%.qrc $(addsuffix /Messages,$(addprefix $(MESSAGES_TARGET)/,$(MESSAGES_LANGUAGES)))
	$(VQ)echo "     RCC: $<"
	$(Q)$(RCC) -name $* -o $@ $<

# builtin resource sources
S_RESOURCE := $(foreach QRC,$(QRC_FILE),$(OBJROOT)/$(QRC).qrc.cpp)

# ----------------------------------------------------------------------------
# Meta Object Compiler (MOC) handling
# ----------------------------------------------------------------------------

MOC_CLASS := actions \
	application \
	bookmarks \
	cookies \
	corewindow \
	global_history \
	listselection \
	local_history \
	page_info \
	scaffolding \
	scaffoldstyle \
	statussplitter \
	settings \
	urlbar \
	widget \
	window

# Generate cpp from h via Qt's Meta Object Compiler
$(OBJROOT)/%.moc.cpp: $(FRONTEND_SOURCE_DIR)/%.cls.h
	$(VQ)echo "     MOC: $<"
	$(Q)$(MOC) $(IFLAGS) $< -o $@

S_MOC := $(foreach CLS,$(MOC_CLASS),$(OBJROOT)/$(CLS).moc.cpp)

# ----------------------------------------------------------------------------
# Source file setup
# ----------------------------------------------------------------------------

# S_FRONTEND are sources purely for the QT frontend
S_FRONTEND := actions.cpp \
	application.cpp \
	bitmap.cpp \
	bookmarks.cpp \
	cookies.cpp \
	corewindow.cpp \
	fetch.cpp \
	global_history.cpp \
	keymap.cpp \
	layout.cpp \
	listselection.cpp \
	local_history.cpp \
	page_info.cpp \
	main.cpp \
	misc.cpp \
	plotters.cpp \
	resources.cpp \
	scaffolding.cpp \
	settings.cpp \
	statussplitter.cpp \
	urlbar.cpp \
	window.cpp \
	widget.cpp


# This is the final source build list
# Note this is deliberately *not* expanded here as common and image
#   are not yet available
SOURCES = $(S_COMMON) $(S_IMAGE) $(S_BROWSER) $(S_RESOURCE) $(S_FRONTEND) $(S_MOC)
