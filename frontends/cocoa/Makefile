# ----------------------------------------------------------------------------
# Mac OS X target setup
# ----------------------------------------------------------------------------

POSTEXES += NetSurf.app

# shut up zconf.h and zlib.h
#CFLAGS += -D_LARGEFILE64_SOURCE=1

# add Mac Ports include and library paths for openssl
ifneq ($(shell test -d /opt/local && echo 'yes'),)
  LDFLAGS += -L/opt/local/lib
  CFLAGS += -I/opt/local/include
endif

ifeq ($(SDK_VERSION),)
  # if no SDK_VERSION has been specified select one from those available
  SDK_PARAM := $(shell xcodebuild -showsdks | awk '/^$$/{p=0};p; /OS X SDKs:/{p=1}' | tail -1 | cut -f3)
  SDK_VERSION := $(MACOSX_VERSION)
else
  SDK_PARAM := -sdk macosx$(SDK_VERSION)
endif

SDK_PATH ?= $(shell xcodebuild -version $(SDK_PARAM) Path)
SDK_FLAGS := -isysroot $(SDK_PATH) -mmacosx-version-min=$(SDK_VERSION)
CFLAGS :=  $(SDK_FLAGS) $(CFLAGS)
LDFLAGS :=  $(SDK_FLAGS) -Wl,-syslibroot,$(SDK_PATH) $(LDFLAGS)
CXXFLAGS :=  $(SDK_FLAGS) $(CXXFLAGS)

# for timerisset()
CFLAGS += -D_DARWIN_C_SOURCE

LDFLAGS += -L/usr/lib
LDFLAGS += -L/usr/X11/lib
LDFLAGS += -lm -lcurl
LDFLAGS += -lssl -lcrypto

CFLAGS += -Dnscocoa -D_BSD_SOURCE -D_POSIX_C_SOURCE -std=c99 -g -Os

CFLAGS += -I/usr/X11/include
CFLAGS += -include cocoa/Prefix.pch

VERSION_FULL := $(shell sed -n '/_version.*=.*"/{s/.*"\(.*\)".*/\1/;p;}' desktop/version.c)
VERSION_MAJ := $(shell sed -n '/_major/{s/.* = \([0-9]*\).*/\1/;p;}' desktop/version.c)
VERSION_MIN := $(shell sed -n '/_minor/{s/.* = \([0-9]*\).*/\1/;p;}' desktop/version.c)

LDFLAGS += -Wl,-framework,Cocoa -Wl,-framework,Carbon $(NETLDFLAGS)

$(eval $(call feature_enabled,IMAGEIO,-DWITH_APPLE_IMAGE,,Apple ImageIO       ))

ifneq ($(UNIVERSAL),)
    UNIVERSAL_FLAGS := $(foreach arch,$(UNIVERSAL),-arch $(arch) )
    CFLAGS += $(UNIVERSAL_FLAGS)
	LDFLAGS += $(UNIVERSAL_FLAGS)
	CXXFLAGS += $(UNIVERSAL_FLAGS)
endif

# ----------------------------------------------------------------------------
# Source file setup
# ----------------------------------------------------------------------------

# sources purely for the Mac OS X build
S_FRONTEND := \
	BookmarksController.m		\
	BrowserView.m			\
	BrowserViewController.m		\
	BrowserWindowController.m	\
	BrowserWindow.m			\
	DownloadWindowController.m	\
	NetSurfAppDelegate.m		\
	NetsurfApp.m			\
	PreferencesWindowController.m	\
	ScrollableView.m		\
	SearchWindowController.m	\
	URLFieldCell.m			\
	Tree.m				\
	TreeView.m			\
	HistoryView.m			\
	HistoryWindowController.m	\
	FormSelectMenu.m		\
	bitmap.m			\
	fetch.m				\
	font.m				\
	gui.m				\
	plotter.m			\
	schedule.m			\
	selection.m			\
	ArrowBox.m			\
	ArrowWindow.m			\
	BlackScroller.m			\
	LocalHistoryController.m	\
	apple_image.m

S_TABBAR := \
	NSBezierPath_AMShading.m	\
	NSString_AITruncation.m		\
	PSMOverflowPopUpButton.m	\
	PSMProgressIndicator.m		\
	PSMRolloverButton.m		\
	PSMTabBarCell.m			\
	PSMTabBarControl.m		\
	PSMTabBarController.m		\
	PSMTabDragAssistant.m		\
	PSMTabDragView.m		\
	PSMTabDragWindow.m		\
	PSMTabDragWindowController.m	\
	PSMUnifiedTabStyle.m

S_FRONTEND += $(addprefix PSMTabBarControl/,$(S_TABBAR))

# This is the final source build list
# Note this is deliberately *not* expanded here as common and image
#   are not yet available
SOURCES = $(addprefix $(shell pwd)/,$(S_COMMON) $(S_IMAGE) $(S_BROWSER) $(S_FRONTEND))

# Since we prefix the sources with the pwd, also create a special
# prefixed rule so that the testament is run
$(shell pwd)/content/fetchers/about.c: testament

EXETARGET := NetSurf

S_XIBS := \
	MainMenu.xib		\
	Browser.xib 		\
	BrowserWindow.xib	\
	DownloadWindow.xib	\
	SearchWindow.xib	\
	PreferencesWindow.xib	\
	HistoryWindow.xib	\
	BookmarksWindow.xib	\
	LocalHistoryPanel.xib

R_RESOURCES := \
	default.css		\
	adblock.css		\
	internal.css		\
	quirks.css		\
	NetSurf.icns		\
	HomeTemplate.pdf	\
	Icons			\
	ca-bundle		\
	netsurf.png


TABBAR_RESOURCES := \
	AquaTabClose_Front_Pressed.png		\
	AquaTabClose_Front_Rollover.png		\
	AquaTabClose_Front.png			\
	AquaTabCloseDirty_Front_Pressed.png	\
	AquaTabCloseDirty_Front_Rollover.png	\
	AquaTabCloseDirty_Front.png		\
	AquaTabNew.png				\
	AquaTabNewPressed.png			\
	AquaTabNewRollover.png			\
	overflowImage.png			\
	overflowImagePressed.png		\
	pi.png

R_RESOURCES := $(addprefix $(FRONTEND_RESOURCES_DIR)/,$(R_RESOURCES))

R_RESOURCES += $(addprefix $(FRONTEND_SOURCE_DIR)/PSMTabBarControl/Images/,$(TABBAR_RESOURCES))

LANGUAGES := de en fr it nl
LOCALIZED_RESOURCES := Localizable.strings

#languiage project macro
# $1 is language name
# $2 is list of resources per language 
define make_lproj 
R_RESOURCES += $$(OBJROOT)/$(1).lproj
$$(OBJROOT)/$(1).lproj: $(2)
	$(VQ)echo Bundling language $(1)
	$(Q)mkdir -p $$@
	$(Q)cp -pLR $(2) $$@
	$(Q)$(SPLIT_MESSAGES) -l $(1) -p cocoa -f messages resources/FatMessages > $$@/Messages
endef

# compile_xib (xib) (lang)
define compile_xib
$$(OBJROOT)/$(2).lproj: $$(OBJROOT)/$(2).lproj/$(1:.xib=.nib)

$$(OBJROOT)/$(2).lproj/$(1:.xib=.nib): $(FRONTEND_RESOURCES_DIR)/$(1) $$(OBJROOT)/created
	$(VQ)echo Compiling XIB $(1) for language $(2)
	$(Q)mkdir -p $$(OBJROOT)/$(2).lproj
	$(Q)$(FRONTEND_SOURCE_DIR)/compile-xib.sh $(FRONTEND_RESOURCES_DIR)/$(1) $(2) $$@

ifeq ($(wildcard $(FRONTEND_RESOURCES_DIR)/$(2).lproj/$(1).strings),$(FRONTEND_RESOURCES_DIR)/$(2).lproj/$(1).strings)
$$(OBJROOT)/$(2).lproj/$(1:.xib=.nib): $(FRONTEND_RESOURCES_DIR)/$(2).lproj/$(1).strings
endif

endef

$(foreach lang,$(LANGUAGES),$(eval $(call make_lproj,$(lang),$(addprefix $(FRONTEND_RESOURCES_DIR)/$(lang).lproj/,$(LOCALIZED_RESOURCES)))))
$(foreach lang,$(LANGUAGES),$(foreach xib,$(S_XIBS),$(eval $(call compile_xib,$(xib),$(lang)))))

# ----------------------------------------------------------------------------
# Install target
# ----------------------------------------------------------------------------

install-cocoa: NetSurf.app

NetSurf.app: NetSurf $(FRONTEND_SOURCE_DIR)/Makefile $(R_RESOURCES) NetSurf.app/Contents/Info.plist
	$(VQ)echo Assembling NetSurf.app bundle
	$(Q)mkdir -p NetSurf.app/Contents/MacOS
	$(Q)cp NetSurf NetSurf.app/Contents/MacOS
	$(Q)rm -rf NetSurf.app/Contents/Resources
	$(Q)mkdir -p NetSurf.app/Contents/Resources
	$(Q)cp -pLR $(R_RESOURCES) NetSurf.app/Contents/Resources
	$(Q)echo 'APPL????' > NetSurf.app/Contents/PkgInfo

NetSurf.app/Contents/Info.plist: $(FRONTEND_RESOURCES_DIR)/NetSurf-Info.plist $(FRONTEND_SOURCE_DIR)/Makefile
	$(VQ)echo Generating Info.plist
	$(Q)mkdir -p NetSurf.app/Contents
	$(Q)sed -e 's/$${EXECUTABLE_NAME}/$(EXETARGET)/' \
	    -e 's/$${PRODUCT_NAME.*}/$(EXETARGET)/' \
	    -e 's/$${MACOSX_DEPLOYMENT_TARGET}/$(MACOSX_VERSION)/' \
	    -e 's/$${NETSURF_VERSION}/$(VERSION_FULL)/' \
		-e 's/$${NETSURF_SHORT_VERSION}/$(VERSION_MAJ).$(VERSION_MIN)/' \
	   < $(FRONTEND_RESOURCES_DIR)/NetSurf-Info.plist > NetSurf.app/Contents/Info.plist

# ----------------------------------------------------------------------------
# Package target
# ----------------------------------------------------------------------------

package-cocoa: NetSurf.dmg

.INTERMEDIATE: NetSurf.tmp.dmg

NetSurf.tmp.dmg: NetSurf.app
	hdiutil create -size 8m -fs HFS+ -volname "NetSurf" $@
	sleep 2
	hdiutil attach $@
	sleep 2
	cp -pPR $^ /Volumes/NetSurf/
	hdiutil detach $$(echo $$(hdiutil attach $@ | cut -f 1)  | cut -f 1 -d ' ')
	sleep 2

NetSurf.dmg: NetSurf.tmp.dmg
	hdiutil convert $^ -format UDZO -o $@

CLEANS += clean-package-cocoa

clean-package-cocoa:
	$(VQ)echo "   CLEAN: NetSurf.tmp.dmg"
	$(Q)$(RM) NetSurf.tmp.dmg
	$(VQ)echo "   CLEAN: NetSurf.dmg"
	$(Q)$(RM) NetSurf.dmg 
	$(VQ)echo "   CLEAN: NetSurf.app"
	$(Q)$(RM) -r NetSurf.app 
