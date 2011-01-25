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

#import "desktop/browser.h"
#import "desktop/plotters.h"
#import "content/urldb.h"
#import "image/bitmap.h"

#import <Cocoa/Cocoa.h>

/* In platform specific thumbnail.c. */
bool thumbnail_create(struct hlcache_handle *content, struct bitmap *bitmap,
					  const char *url)
{
	CGColorSpaceRef cspace = CGColorSpaceCreateWithName( kCGColorSpaceGenericRGB );
	CGContextRef bitmapContext = CGBitmapContextCreate( bitmap_get_buffer( bitmap ), 
													   bitmap_get_width( bitmap ), bitmap_get_height( bitmap ), 
													   bitmap_get_bpp( bitmap ) * 8 / 4, 
													   bitmap_get_rowstride( bitmap ), 
													   cspace, kCGImageAlphaNoneSkipLast );
	CGColorSpaceRelease( cspace );
	

	size_t width = MIN( content_get_width( content ), 1024 );
	size_t height = MIN( content_get_height( content ), 768 );
	
	CGContextTranslateCTM( bitmapContext, 0, bitmap_get_height( bitmap ) );
	CGContextScaleCTM( bitmapContext, (CGFloat)bitmap_get_width( bitmap ) / width, -(CGFloat)bitmap_get_height( bitmap ) / height );
	
	[NSGraphicsContext setCurrentContext: [NSGraphicsContext graphicsContextWithGraphicsPort: bitmapContext flipped: YES]];
	
	content_redraw( content, 0, 0, content_get_width( content ), content_get_height( content ), 
				   0, 0, content_get_width( content ), content_get_height( content ), 
				   1.0, 0xFFFFFFFF );
	
	[NSGraphicsContext setCurrentContext: nil];
	CGContextRelease( bitmapContext );
	
	bitmap_modified( bitmap );
	
	if (NULL != url) urldb_set_thumbnail( url, bitmap );
	
	return true;

}

