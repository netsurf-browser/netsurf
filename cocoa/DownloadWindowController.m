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

#import "DownloadWindowController.h"

#import "desktop/download.h"
#import "desktop/gui.h"

#define UNIMPL() NSLog( @"Function '%s' unimplemented", __func__ )

@interface DownloadWindowController ()

@property (readwrite, retain, nonatomic) NSFileHandle *outputFile;
@property (readwrite, retain, nonatomic) NSMutableData *savedData;
@property (readwrite, copy, nonatomic) NSDate *startDate;

- (void)savePanelDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (void)alertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;

- (BOOL) receivedData: (NSData *)data;

- (void) showError: (NSString *)error;

@end


@implementation DownloadWindowController

- initWithContext: (struct download_context *)ctx;
{
	if ((self = [super initWithWindowNibName: @"DownloadWindow"]) == nil) return nil;
	
	context = ctx;
	totalSize = download_context_get_total_length( context );
	[self setURL: [NSURL URLWithString: [NSString stringWithUTF8String: download_context_get_url( context )]]];
	[self setMIMEType: [NSString stringWithUTF8String: download_context_get_mime_type( context )]];
	[self setStartDate: [NSDate date]];
	
	return self;
}

- (void) dealloc;
{
	download_context_destroy( context );
	
	[self setURL: nil];
	[self setMIMEType: nil];
	[self setSaveURL: nil];
	[self setOutputFile: nil];
	[self setSavedData: nil];
	[self setStartDate: nil];
	
	[super dealloc];
}

- (void) abort;
{
	download_context_abort( context );
}

- (void) askForSave;
{
	[[NSSavePanel savePanel] beginSheetForDirectory: nil 
											   file: [[url path] lastPathComponent]
									 modalForWindow: [self window] 
									  modalDelegate: self 
									 didEndSelector: @selector(savePanelDidEnd:returnCode:contextInfo:) 
										contextInfo: NULL];
}

- (void)savePanelDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
{
	if (returnCode == NSCancelButton) {
		[self abort];
		return;
	}

	NSURL *targetURL = [sheet URL];
	NSString *path = [targetURL path];
	
	[[NSFileManager defaultManager] createFileAtPath: path contents: nil attributes: nil];
	[self setOutputFile: [NSFileHandle fileHandleForWritingAtPath: path]];
	[self setSaveURL: targetURL];
	
	NSWindow *win = [self window];
	[win setRepresentedURL: targetURL];
	[win setTitle: [self fileName]];
	
	if (nil == outputFile) {
		[self performSelector: @selector(showError:) withObject: @"Cannot create file" afterDelay: 0];
		return;
	}

	if (nil != savedData) {
		[outputFile writeData: savedData];
		[self setSavedData: nil];
	}
}

- (BOOL) receivedData: (NSData *)data;
{
	if (outputFile) {
		[outputFile writeData: data];
	} else {
		if (nil == savedData) [self setSavedData: [NSMutableData data]];
		[savedData appendData: data];
	}
	
	[self setReceivedSize: receivedSize + [data length]];
	
	return YES;
}

- (void) showError: (NSString *)error;
{
	NSAlert *alert = [NSAlert alertWithMessageText: @"Error" defaultButton: @"OK" 
								   alternateButton: nil otherButton: nil 
						 informativeTextWithFormat: @"%@", error];
	
	[alert beginSheetModalForWindow: [self window] modalDelegate: self 
					 didEndSelector: @selector(alertDidEnd:returnCode:contextInfo:)
						contextInfo: NULL];
}

- (void)alertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo;
{
	[self abort];
}

#pragma mark -
#pragma mark Properties

@synthesize URL = url;
@synthesize MIMEType = mimeType;
@synthesize totalSize;
@synthesize saveURL;
@synthesize outputFile;
@synthesize savedData;
@synthesize receivedSize;
@synthesize startDate;

+ (NSSet *) keyPathsForValuesAffectingStatusText;
{
	return [NSSet setWithObjects: @"totalSize", @"receivedSize", nil];
}

static NSString *cocoa_file_size_string( float size )
{
    if (size < 1023) return [NSString stringWithFormat:@"%1.0f bytes",size];

    size /= 1024;
    if (size < 1023) return [NSString stringWithFormat:@"%1.1f KiB", size];
    
	size /= 1024;
    if (size < 1023) return [NSString stringWithFormat:@"%1.1f MiB", size];

    size /= 1024;
    return [NSString stringWithFormat:@"%1.1f GiB", size];
}

- (NSString *) statusText;
{
	NSString *speedString = @"";
	
	float elapsedTime = [[NSDate date] timeIntervalSinceDate: startDate];
	if (elapsedTime >= 0.1) {
		float speed = (float)receivedSize / elapsedTime;
		speedString = [NSString stringWithFormat: @" (%@/s)", cocoa_file_size_string( speed )];
	}
	
	return [NSString stringWithFormat: @"%@ of %@%@", cocoa_file_size_string( receivedSize ), 
			cocoa_file_size_string( totalSize ), speedString];
}

+ (NSSet *) keyPathsForValuesAffectingFileName;
{
	return [NSSet setWithObject: @"saveURL"];
}

- (NSString *) fileName;
{
	return [[saveURL path] lastPathComponent];
}

+ (NSSet *) keyPathsForValuesAffectingIcon;
{
	return [NSSet setWithObject: @"saveURL"];
}

- (NSImage *) icon;
{
	return saveURL != nil ? [[NSWorkspace sharedWorkspace] iconForFile: [saveURL path]] : nil;
}


#pragma mark -
#pragma mark NetSurf interface functions

struct gui_download_window *gui_download_window_create(download_context *ctx,
													   struct gui_window *parent)
{
	DownloadWindowController * const window = [[DownloadWindowController alloc] initWithContext: ctx];
	[window askForSave];
	
	return (struct gui_download_window *)window;
}

nserror gui_download_window_data(struct gui_download_window *dw, 
								 const char *data, unsigned int size)
{
	DownloadWindowController * const window = (DownloadWindowController *)dw;
	return [window receivedData: [NSData dataWithBytes: data length: size]] ? NSERROR_OK : NSERROR_SAVE_FAILED;
}

void gui_download_window_error(struct gui_download_window *dw,
							   const char *error_msg)
{
	DownloadWindowController * const window = (DownloadWindowController *)dw;
	[window showError: [NSString stringWithUTF8String: error_msg]];
}

void gui_download_window_done(struct gui_download_window *dw)
{
	DownloadWindowController * const window = (DownloadWindowController *)dw;
	[window release];
}

@end
