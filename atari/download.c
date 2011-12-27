/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <windom.h>

#include "content/urldb.h"
#include "content/fetch.h"
#include "desktop/gui.h"
#include "desktop/history_core.h"
#include "desktop/netsurf.h"
#include "desktop/options.h"
#include "desktop/save_complete.h"
#include "desktop/selection.h"
#include "desktop/textinput.h"
#include "desktop/download.h"
#include "render/html.h"
#include "utils/url.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "atari/gui.h"
#include "atari/misc.h"
#include "atari/res/netsurf.rsh"
#include "atari/download.h"
#include "atari/options.h"
#include "atari/osspec.h"

/*TODO: get filename from core. */

extern struct gui_window * input_window;

static void gui_download_window_destroy( struct gui_download_window * gdw );

static void __CDECL evnt_bt_abort_click
(
 	WINDOW *win,
	int index,
	int unused,
	void * data
)
{
	struct gui_download_window * dw = (struct gui_download_window *)data;
	assert( dw != NULL );
	ObjcChange( OC_FORM, win, index, ~SELECTED, TRUE);
	if( dw->status != NSATARI_DOWNLOAD_CANCELED ){
		dw->abort = true;
	}
}

static void __CDECL evnt_cbrdy_click
(
	WINDOW *win,
	int index,
	int unused,
	void * data
)
{
	struct gui_download_window * dw = (struct gui_download_window *)data;
	assert( dw != NULL );
	if( dw->status == NSATARI_DOWNLOAD_COMPLETE ){
		ApplWrite( _AESapid, WM_CLOSED, win->handle, 0,0,0,0);
	}
}

static void __CDECL evnt_close( WINDOW *win, short buff[8], void * data)
{
	struct gui_download_window * dw = (struct gui_download_window *)data;
	assert( dw != NULL );
	gui_download_window_destroy( dw );
	ApplWrite( _AESapid, WM_DESTROY, win->handle, 0,0,0,0);
}

static void gui_download_window_destroy( struct gui_download_window * gdw )
{

	if( gdw->status == NSATARI_DOWNLOAD_WORKING ){
		download_context_abort( gdw->ctx );
	}
	download_context_destroy( gdw->ctx );
	if( gdw->form != NULL ){
		/* first destroy the form, so that it won't acces the gdw members */
		ApplWrite( _AESapid, WM_DESTROY, gdw->form->handle, 0,0,0,0);
		EvntWindom( MU_MESAG );
	}
	if( gdw->destination ) {
		free( gdw->destination );
	}
	if( gdw->domain ) {
		free( gdw->domain );
	}
	if( gdw->url ) {
		free( gdw->url );
	}
	if( gdw->fd != NULL ){
		fclose(gdw->fd);
		gdw->fd = NULL;
	}
	if( gdw->fbuf != NULL ){
		free( gdw->fbuf );
	}

	free( gdw );
}

struct gui_download_window *gui_download_window_create(download_context *ctx,
		struct gui_window *parent)
{

	char *filename;
	char *destination;
	const char * path = option_downloads_path;
	const char * url;
	struct gui_download_window * gdw;

	/* TODO: Implement real form and use messages file strings! */
	if( form_alert(2, "[2][Accept Download?][Yes|No]") == 2){
		return( NULL );
	}

	OBJECT * tree = get_tree(DOWNLOAD);
	if( tree == NULL )
		return( NULL );
	gdw = calloc( 1, sizeof(struct gui_download_window) );
	if( gdw == NULL )
		return( NULL );
	gdw->ctx = ctx;
	gdw->abort = false;
	gdw->start = clock() / CLOCKS_PER_SEC;
	gdw->lastrdw = 0;
	gdw->status = NSATARI_DOWNLOAD_WORKING;
	gdw->parent = parent;
	gdw->fbufsize = MAX(BUFSIZ, 48000);
	gdw->size_downloaded = 0;
	gdw->size_total = download_context_get_total_length(ctx);
	url = download_context_get_url(ctx);

	gdw->url = calloc(1, strlen(url) + 1 );
	if( gdw->url == NULL ){
		gui_download_window_destroy(gdw);
		return( NULL );
	}

	if (url_nice(url, &filename, false) != URL_FUNC_OK) {
		filename = strdup("unknown");
		if (filename == NULL) {
			gui_download_window_destroy(gdw);
			return NULL;
		}
	}

	if (url_host(url, &gdw->domain) != URL_FUNC_OK) {
		gdw->domain = strdup("unknown");
		if (gdw->domain == NULL) {
			free(filename);
			gui_download_window_destroy(gdw);
			return NULL;
		}
	}

	char * tpath = alloca(strlen(filename) + strlen(path) + 2 );
	char * tpath2 = alloca(PATH_MAX);
	strcpy( tpath, path );
	if( path[strlen(path)-1] != '/' &&  path[strlen(path)-1] != '\\' ) {
		strcat( tpath, "/");
	}
	strcat( tpath, filename );
	gemdos_realpath(tpath, tpath2);
	gdw->destination = malloc(strlen(tpath2) + 2);
	strcpy(gdw->destination, tpath2);
	gdw->fd = fopen(gdw->destination, "wb" );
	if( gdw->fd == NULL ){
		free( filename );
		gui_download_window_destroy(gdw);
		return( NULL );
	}
	gdw->fbuf = malloc( gdw->fbufsize+1 );
	if( gdw->fbuf != NULL ){
		setvbuf( gdw->fd, gdw->fbuf, _IOFBF, gdw->fbufsize );
	}
	gdw->form = mt_FormCreate( &app, tree, WAT_FORM,
								NULL, (char*)"Download",
								NULL, true, true );
	if( gdw->form == NULL || gdw->fd == NULL ){
		free( filename );
		gui_download_window_destroy(gdw);
		return( NULL );
	}

	tree = ObjcTree(OC_FORM, gdw->form );
	ObjcAttachFormFunc( gdw->form, DOWNLOAD_BT_ABORT,
		evnt_bt_abort_click, gdw
	);
	ObjcAttachFormFunc( gdw->form, DOWNLOAD_CB_CLOSE_RDY,
		evnt_cbrdy_click, gdw
	);
	EvntDataAdd( gdw->form, WM_CLOSED, evnt_close, gdw, EV_TOP);
	strncpy((char*)&gdw->lbl_file, filename, MAX_SLEN_LBL_FILE-1);
	ObjcString( tree, DOWNLOAD_FILENAME, (char*)&gdw->lbl_file );
	ObjcString( tree, DOWNLOAD_LBL_BYTES, (char*)&gdw->lbl_done );
	ObjcString( tree, DOWNLOAD_LBL_PERCENT, (char*)&gdw->lbl_percent );
	ObjcString( tree, DOWNLOAD_LBL_SPEED, (char*)&gdw->lbl_speed );

	free( filename );
	LOG(("created download: %s (total size: %d)",
		gdw->destination, gdw->size_total
	));

	return gdw;
}

nserror gui_download_window_data(struct gui_download_window *dw,
		const char *data, unsigned int size)
{
	uint32_t p = 0;
	uint32_t tnow = clock() / CLOCKS_PER_SEC;
	uint32_t sdiff = tnow - dw->start;
	float speed;
	float pf = 0;
	OBJECT * tree;

	if( dw->abort == true ){
		dw->status = NSATARI_DOWNLOAD_CANCELED;
		dw->abort = false;
		download_context_abort( dw->ctx );
		ObjcChange( OC_FORM, dw->form, DOWNLOAD_BT_ABORT, DISABLED, TRUE);
		return( NSERROR_OK );
	}

	/* save data */
	fwrite( data , size, sizeof(unsigned char),dw->fd );
	dw->size_downloaded += size;

	/* Update the progress bar... */
	if( tnow - dw->lastrdw > 1 ) {
		dw->lastrdw = tnow;
		tree = ObjcTree(OC_FORM, dw->form );
		if( dw->size_total > 0 ){
			p = (dw->size_downloaded *100) / dw->size_total;
		}
		speed = dw->size_downloaded / sdiff;
		tree[DOWNLOAD_PROGRESS_DONE].ob_width = MAX( MIN( p*(DOWNLOAD_BAR_MAX/100), DOWNLOAD_BAR_MAX ), 1);
		if( dw->size_total > 0 ){
			snprintf( (char*)&dw->lbl_percent, MAX_SLEN_LBL_PERCENT,
				"%lu%s", p, "%"
			);
		} else {
			snprintf( (char*)&dw->lbl_percent, MAX_SLEN_LBL_PERCENT,
				"%s", "?%"
			);
		}
		snprintf( (char*)&dw->lbl_speed, MAX_SLEN_LBL_SPEED, "%s/s",
			human_friendly_bytesize(speed)
		);
		snprintf( (char*)&dw->lbl_done, MAX_SLEN_LBL_DONE, "%s / %s",
			human_friendly_bytesize(dw->size_downloaded),
			(dw->size_total>0) ? human_friendly_bytesize(dw->size_total) : "?"
		);
		ObjcString( tree, DOWNLOAD_LBL_BYTES, (char*)&dw->lbl_done );
		ObjcString( tree, DOWNLOAD_LBL_PERCENT, (char*)&dw->lbl_percent );
		ObjcString( tree, DOWNLOAD_LBL_SPEED, (char*)&dw->lbl_speed );
		snd_rdw( dw->form );
	}
	return NSERROR_OK;
}

void gui_download_window_error(struct gui_download_window *dw,
                               const char *error_msg)
{
	LOG(("%s", error_msg));
	strncpy((char*)&dw->lbl_file, error_msg, MAX_SLEN_LBL_FILE-1);
	dw->status = NSATARI_DOWNLOAD_ERROR;
	snd_rdw( dw->form );
	gui_window_set_status(input_window, messages_get("Done") );
}

void gui_download_window_done(struct gui_download_window *dw)
{
	LOG((""));
	dw->status = NSATARI_DOWNLOAD_COMPLETE;
	if( dw->fd != NULL ) {
		fclose( dw->fd );
		dw->fd = NULL;
	}
	OBJECT * tree = ObjcTree(OC_FORM, dw->form );
	tree[DOWNLOAD_PROGRESS_DONE].ob_width = DOWNLOAD_BAR_MAX;
	snprintf( (char*)&dw->lbl_percent, MAX_SLEN_LBL_PERCENT,
		"%lu%s", 100, "%"
	);
	snprintf( (char*)&dw->lbl_done, MAX_SLEN_LBL_DONE, "%s / %s",
		human_friendly_bytesize(dw->size_downloaded),
		(dw->size_total>0) ? human_friendly_bytesize(dw->size_total) : human_friendly_bytesize(dw->size_downloaded)
	);
	ObjcString( tree, DOWNLOAD_LBL_BYTES, (char*)&dw->lbl_done );
	ObjcString( tree, DOWNLOAD_LBL_PERCENT, (char*)&dw->lbl_percent );
	ObjcChange( OC_FORM, dw->form, DOWNLOAD_BT_ABORT, DISABLED, FALSE);
	snd_rdw( dw->form );
	if( (tree[DOWNLOAD_CB_CLOSE_RDY].ob_state & SELECTED) != 0 ) {
		ApplWrite( _AESapid, WM_CLOSED, dw->form->handle, 0,0,0,0);
	}
	gui_window_set_status(input_window, messages_get("Done") );
}
