# ----------------------------------------------------------------------------
# Framebuffer target setup
# ----------------------------------------------------------------------------

CFLAGS += -Dnsframebuffer 

#resource path
CFLAGS += '-DNETSURF_FB_RESPATH="$(NETSURF_FB_RESPATH)"'

# compile time font locations
CFLAGS += '-DNETSURF_FB_FONTPATH="$(NETSURF_FB_FONTPATH)"'
CFLAGS += '-DNETSURF_FB_FONT_SANS_SERIF="$(NETSURF_FB_FONT_SANS_SERIF)"'
CFLAGS += '-DNETSURF_FB_FONT_SANS_SERIF_BOLD="$(NETSURF_FB_FONT_SANS_SERIF_BOLD)"'
CFLAGS += '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC="$(NETSURF_FB_FONT_SANS_SERIF_ITALIC)"'
CFLAGS += '-DNETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD="$(NETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD)"'
CFLAGS += '-DNETSURF_FB_FONT_SERIF="$(NETSURF_FB_FONT_SERIF)"'
CFLAGS += '-DNETSURF_FB_FONT_SERIF_BOLD="$(NETSURF_FB_FONT_SERIF_BOLD)"'
CFLAGS += '-DNETSURF_FB_FONT_MONOSPACE="$(NETSURF_FB_FONT_MONOSPACE)"'
CFLAGS += '-DNETSURF_FB_FONT_MONOSPACE_BOLD="$(NETSURF_FB_FONT_MONOSPACE_BOLD)"'
CFLAGS += '-DNETSURF_FB_FONT_CURSIVE="$(NETSURF_FB_FONT_CURSIVE)"'
CFLAGS += '-DNETSURF_FB_FONT_FANTASY="$(NETSURF_FB_FONT_FANTASY)"'

CFLAGS += -std=c99 -g -Dsmall \
	  -D_BSD_SOURCE \
	  -D_DEFAULT_SOURCE \
	  -D_XOPEN_SOURCE=600 \
	  -D_POSIX_C_SOURCE=200809L

LDFLAGS += -lm

# non optional pkg-configed libs
LDFLAGS += -Wl,--whole-archive
$(eval $(call pkg_config_find_and_add,libnsfb,libnsfb))
LDFLAGS += -Wl,--no-whole-archive

# freetype is optional but does not use pkg-config
ifeq ($(NETSURF_FB_FONTLIB),freetype)
  CFLAGS += -DFB_USE_FREETYPE $(shell freetype-config --cflags)
  LDFLAGS += $(shell freetype-config --libs)
endif


# ----------------------------------------------------------------------------
# built-in resource setup
# ----------------------------------------------------------------------------

FB_IMAGE_left_arrow := icons/back.png
FB_IMAGE_right_arrow := icons/forward.png
FB_IMAGE_reload := icons/reload.png
FB_IMAGE_stop_image := icons/stop.png
FB_IMAGE_history_image := icons/history.png

FB_IMAGE_left_arrow_g := icons/back_g.png
FB_IMAGE_right_arrow_g := icons/forward_g.png
FB_IMAGE_reload_g := icons/reload_g.png
FB_IMAGE_stop_image_g := icons/stop_g.png
FB_IMAGE_history_image_g := icons/history_g.png

FB_IMAGE_scrolll := icons/scrolll.png
FB_IMAGE_scrollr := icons/scrollr.png
FB_IMAGE_scrollu := icons/scrollu.png
FB_IMAGE_scrolld := icons/scrolld.png

FB_IMAGE_osk_image := icons/osk.png

FB_IMAGE_pointer_image := pointers/default.png
FB_IMAGE_hand_image := pointers/point.png
FB_IMAGE_caret_image := pointers/caret.png
FB_IMAGE_menu_image := pointers/menu.png
FB_IMAGE_progress_image := pointers/progress.png
FB_IMAGE_move_image := pointers/move.png

FB_IMAGE_throbber0 := throbber/throbber0.png
FB_IMAGE_throbber1 := throbber/throbber1.png
FB_IMAGE_throbber2 := throbber/throbber2.png
FB_IMAGE_throbber3 := throbber/throbber3.png
FB_IMAGE_throbber4 := throbber/throbber4.png
FB_IMAGE_throbber5 := throbber/throbber5.png
FB_IMAGE_throbber6 := throbber/throbber6.png
FB_IMAGE_throbber7 := throbber/throbber7.png
FB_IMAGE_throbber8 := throbber/throbber8.png

# local compiler flags
ifeq ($(HOST),OpenBSD)
    HOST_CFLAGS += $(shell $(PKG_CONFIG) --cflags libpng)
    HOST_LDFLAGS += $(shell $(PKG_CONFIG) --libs libpng)
else
    HOST_CFLAGS +=
    HOST_LDFLAGS += -lpng
endif

# Host tool to convert image bitmaps to source code.
#
# convert_image dependd on fb_bitmap.h so that if we change that
# header, we get new images built.
$(TOOLROOT)/convert_image: $(TOOLROOT)/created $(FRONTEND_SOURCE_DIR)/convert_image.c $(FRONTEND_SOURCE_DIR)/fbtk.h
	$(VQ)echo " HOST CC: $@"
	$(Q)$(HOST_CC) $(HOST_CFLAGS) -o $@ $(FRONTEND_SOURCE_DIR)/convert_image.c $(HOST_LDFLAGS)

# 1: input file
# 2: output file
# 3: bitmap name
define convert_image

S_IMAGES += $(2)

$(2): $(1) $(TOOLROOT)/convert_image
	$(Q)$(TOOLROOT)/convert_image $(1) $(2) $(3)

endef

S_IMAGES :=

$(eval $(foreach V,$(filter FB_IMAGE_%,$(.VARIABLES)),$(call convert_image,$(FRONTEND_RESOURCES_DIR)/$($(V)),$(OBJROOT)/image-$(patsubst FB_IMAGE_%,%,$(V)).c,$(patsubst FB_IMAGE_%,%,$(V)))))


# Internal fonts to generate
FB_FONT_internal_ns-sans := fonts/glyph_data

# Internal font conversion
$(TOOLROOT)/convert_font: $(TOOLROOT)/created $(FRONTEND_SOURCE_DIR)/convert_font.c
	$(VQ)echo " HOST CC: $@"
	$(Q)$(HOST_CC) -o $@ $(FRONTEND_SOURCE_DIR)/convert_font.c

# 1: input file
# 2: output source code file
# 3: output header file
# 4: font name
define convert_font

S_FONTS += $(2)

$(2): $(1) $(TOOLROOT)/convert_font
	$(VQ)echo "    FONT: $(1) ($(4))"
	$(Q)$(TOOLROOT)/convert_font -H $(3) $(1) $(2)

endef

S_FONTS :=

$(eval $(foreach V,$(filter FB_FONT_$(NETSURF_FB_FONTLIB)_%,$(.VARIABLES)),$(call convert_font,$(FRONTEND_RESOURCES_DIR)/$($(V)),$(OBJROOT)/font-$(patsubst FB_FONT_$(NETSURF_FB_FONTLIB)_%,%,$(V)).c,$(OBJROOT)/font-$(patsubst FB_FONT_$(NETSURF_FB_FONTLIB)_%,%,$(V)).h,$(patsubst FB_FONT_$(NETSURF_FB_FONTLIB)_%,%,$(V)))))

# ----------------------------------------------------------------------------
# Source file setup
# ----------------------------------------------------------------------------

# S_FRONTEND are sources purely for the framebuffer build
S_FRONTEND := gui.c framebuffer.c schedule.c bitmap.c fetch.c	\
	findfile.c localhistory.c clipboard.c

# toolkit sources
S_FRAMEBUFFER_FBTK := fbtk.c event.c fill.c bitmap.c user.c window.c 	\
	text.c scroll.c osk.c 

S_FRONTEND += font_$(NETSURF_FB_FONTLIB).c

S_FRONTEND += $(addprefix fbtk/,$(S_FRAMEBUFFER_FBTK))

# This is the final source build list
# Note this is deliberately *not* expanded here as common and image
#   are not yet available
SOURCES = $(S_COMMON) $(S_IMAGE) $(S_BROWSER) $(S_FRONTEND) $(S_IMAGES) $(S_FONTS)
EXETARGET := nsfb

# ----------------------------------------------------------------------------
# Install target
# ----------------------------------------------------------------------------

NETSURF_FRAMEBUFFER_RESOURCE_LIST := adblock.css credits.html	\
	default.css internal.css licence.html			\
	netsurf.png quirks.css welcome.html maps.html Messages

install-framebuffer:
	$(Q)mkdir -p $(DESTDIR)$(NETSURF_FRAMEBUFFER_BIN)
	$(Q)mkdir -p $(DESTDIR)$(NETSURF_FRAMEBUFFER_RESOURCES)
	$(Q)cp -v $(EXETARGET) $(DESTDIR)/$(NETSURF_FRAMEBUFFER_BIN)netsurf-fb
	$(Q)for F in $(NETSURF_FRAMEBUFFER_RESOURCE_LIST); do cp -vL $(FRONTEND_RESOURCES_DIR)/$$F $(DESTDIR)/$(NETSURF_FRAMEBUFFER_RESOURCES); done
	$(Q)$(SPLIT_MESSAGES) -l en -p fb -f messages resources/FatMessages | gzip -9n > $(DESTDIR)$(NETSURF_FRAMEBUFFER_RESOURCES)messages

# ----------------------------------------------------------------------------
# Package target
# ----------------------------------------------------------------------------

package-framebuffer:
