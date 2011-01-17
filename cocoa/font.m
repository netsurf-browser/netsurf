/*
 * Copyright 2011 Sven Weidauer <sven.weidauer@gmail.com>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#import <Cocoa/Cocoa.h>

#include <inttypes.h>

#include <assert.h>

#include "css/css.h"
#include "render/font.h"
#include "desktop/options.h"

#import "font.h"
#import "plotter.h"

static NSLayoutManager *cocoa_prepare_layout_manager( const char *string, size_t length, 
													 const plot_font_style_t *style );
static NSTextStorage *cocoa_text_storage = nil;
static NSTextContainer *cocoa_text_container = nil;

static bool nsfont_width(const plot_font_style_t *style,
						 const char *string, size_t length,
						 int *width)
{
	NSCParameterAssert( NULL != width );
	
	NSLayoutManager *layout = cocoa_prepare_layout_manager( string, length, style );
	*width = NULL != layout ? NSWidth( [layout usedRectForTextContainer: cocoa_text_container] ) : 0;
	return true;
}

static bool nsfont_position_in_string(const plot_font_style_t *style,
									  const char *string, size_t length,
									  int x, size_t *char_offset, int *actual_x)
{
	NSLayoutManager *layout = cocoa_prepare_layout_manager( string, length, style );
	NSUInteger glyphIndex = [layout glyphIndexForPoint: NSMakePoint( x, 0 ) 
									   inTextContainer: cocoa_text_container 
						fractionOfDistanceThroughGlyph: NULL];
	NSUInteger chars = [layout characterIndexForGlyphAtIndex: glyphIndex];

	size_t offset = 0;
	while (chars > 0) {
		uint8_t ch = ((uint8_t *)string)[offset];
		
		if (0xC2 <= ch && ch <= 0xDF) offset += 2;
		else if (0xE0 <= ch && ch <= 0xEF) offset += 3;
		else if (0xF0 <= ch && ch <= 0xF4) offset += 4;
		else offset++;
		
		--chars;
	}
	
	*char_offset = offset;
	*actual_x = [layout locationForGlyphAtIndex: glyphIndex].x;
	
	return true;
}

static bool nsfont_split(const plot_font_style_t *style,
						 const char *string, size_t length,
						 int x, size_t *char_offset, int *actual_x)
{
	nsfont_position_in_string(style, string, length, x, char_offset, 
							  actual_x);
	if (*char_offset == length) return true;

	while ((string[*char_offset] != ' ') && (*char_offset > 0))
		(*char_offset)--;
	
	nsfont_position_in_string(style, string, *char_offset + 1, x, char_offset,
							  actual_x);

	return true;
}


static NSString *cocoa_font_family_name( plot_font_generic_family_t family )
{
	switch (family) {
		case PLOT_FONT_FAMILY_SERIF: return @"Times";
		case PLOT_FONT_FAMILY_SANS_SERIF: return @"Helvetica";
		case PLOT_FONT_FAMILY_MONOSPACE: return @"Courier";
		case PLOT_FONT_FAMILY_CURSIVE: return @"Apple Chancery";
		case PLOT_FONT_FAMILY_FANTASY: return @"Marker Felt";
		default: return nil;
	}
}

static NSFont *cocoa_font_get_nsfont( const plot_font_style_t *style )
{
	return [NSFont fontWithName: cocoa_font_family_name( style->family ) 
						   size: (CGFloat)style->size / FONT_SIZE_SCALE];
}

NSDictionary *cocoa_font_attributes( const plot_font_style_t *style )
{
	return [NSDictionary dictionaryWithObjectsAndKeys: 
			cocoa_font_get_nsfont( style ), NSFontAttributeName, 
			cocoa_convert_colour( style->foreground ), NSForegroundColorAttributeName,
			nil];
}

static NSString *cocoa_string_from_utf8_characters( const char *string, size_t characters )
{
	if (NULL == string || 0 == characters) return nil;
	
	return [[[NSString alloc] initWithBytes: string length:characters encoding:NSUTF8StringEncoding] autorelease];
}


static NSLayoutManager *cocoa_prepare_layout_manager( const char *bytes, size_t length, 
													 const plot_font_style_t *style )
{
	if (NULL == bytes || 0 == length) return nil;
	
	static NSLayoutManager *layout = nil;

	if (nil == layout) {
		cocoa_text_container = [[NSTextContainer alloc] initWithContainerSize: NSMakeSize( CGFLOAT_MAX, CGFLOAT_MAX )];
		[cocoa_text_container setLineFragmentPadding: 0];
		 
		layout = [[NSLayoutManager alloc] init];
		[layout addTextContainer: cocoa_text_container];
		
		cocoa_text_storage = [[NSTextStorage alloc] init];
		[cocoa_text_storage addLayoutManager: layout];
	}
	
	
	NSString *string = cocoa_string_from_utf8_characters( bytes, length );
	if (nil == string) return nil;
	
	NSDictionary *attributes = cocoa_font_attributes( style );
	NSAttributedString *attributedString = [[NSAttributedString alloc] initWithString: string attributes: attributes];
	if (![attributedString isEqualToAttributedString: cocoa_text_storage]) {
		[cocoa_text_storage setAttributedString: attributedString];
		[layout ensureLayoutForTextContainer: cocoa_text_container];
	}
	[attributedString release];
	
	
	return layout;
}

static CGFloat cocoa_font_scale_factor = 1.0;

void cocoa_set_font_scale_factor( float newFactor )
{
	cocoa_font_scale_factor = newFactor;
}

void cocoa_draw_string( int x, int y, const char *bytes, size_t length, const plot_font_style_t *style )
{
	plot_font_style_t scaledStyle = *style;
	scaledStyle.size *= cocoa_font_scale_factor;
	
	NSLayoutManager *layout = cocoa_prepare_layout_manager( bytes, length, &scaledStyle );
	
	if ([cocoa_text_storage length] > 0) {
		NSFont *font = [cocoa_text_storage attribute: NSFontAttributeName atIndex: 0 effectiveRange: NULL];
		CGFloat baseline = [layout defaultBaselineOffsetForFont: font];
		
		NSRange glyphRange = [layout glyphRangeForTextContainer: cocoa_text_container];
		[layout drawGlyphsForGlyphRange: glyphRange atPoint: NSMakePoint( x, y - baseline )];
	}
}

const struct font_functions nsfont = {
	nsfont_width,
	nsfont_position_in_string,
	nsfont_split
};

