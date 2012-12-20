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
#include "atari/osspec.h"

extern struct gui_window * input_window;
extern GRECT desk_area;

static void gui_download_window_destroy( struct gui_download_window * gdw );
static void on_abort_click(struct gui_download_window *dw);
static void on_cbrdy_click(struct gui_download_window *dw);
static void on_close(struct gui_download_window * dw);
static void on_redraw(struct gui_download_window *dw, GRECT *clip);

static short on_aes_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8])
{
    short retval = 0;
    struct gui_download_window *data;

    GRECT clip;

	data = guiwin_get_user_data(win);

    if ((ev_out->emo_events & MU_MESAG) != 0) {
        // handle message
        //printf("download win msg: %d\n", msg[0]);
        switch (msg[0]) {

        case WM_REDRAW:
			clip.g_x = msg[4];
			clip.g_y = msg[5];
			clip.g_w = msg[6];
			clip.g_h = msg[7];
			on_redraw(data, &clip);
            break;

        case WM_CLOSED:
            // TODO: this needs to iterate through all gui windows and
            // check if the rootwin is this window...
			on_close(data);
            break;

        case WM_TOOLBAR:
			switch(msg[4]){

				case DOWNLOAD_BT_ABORT:
					on_abort_click(data);
				break;

				case DOWNLOAD_CB_CLOSE_RDY:
					on_cbrdy_click(data);
				break;

				default: break;
			}
            break;

        default:
            break;
        }
    }
    if ((ev_out->emo_events & MU_KEYBD) != 0) {


    }
    if ((ev_out->emo_events & MU_BUTTON) != 0) {

    }

    return(retval);
}

static void on_redraw(struct gui_download_window *dw, GRECT *clip)
{
	OBJECT *tree = dw->tree;
	GRECT work, visible;
	uint32_t p = 0;

	guiwin_get_grect(dw->guiwin, GUIWIN_AREA_TOOLBAR, &work);
	tree->ob_x = work.g_x;
	tree->ob_y = work.g_y;

	if(!rc_intersect(clip, &work)){
		return;
	}

	/*
		Update the AES Object to reflect current state of download:
	*/
	((TEDINFO *)get_obspec(tree, DOWNLOAD_FILENAME))->te_ptext = &dw->lbl_file;
	((TEDINFO *)get_obspec(tree, DOWNLOAD_LBL_BYTES))->te_ptext = &dw->lbl_done;
	((TEDINFO *)get_obspec(tree, DOWNLOAD_LBL_PERCENT))->te_ptext = &dw->lbl_percent;
	((TEDINFO *)get_obspec(tree, DOWNLOAD_LBL_SPEED))->te_ptext = &dw->lbl_speed;

	if (dw->size_total > 0 ) {
		p = ((double)dw->size_downloaded / (double)dw->size_total * 100);
	}
	tree[DOWNLOAD_PROGRESS_DONE].ob_width = MAX( MIN( p*(DOWNLOAD_BAR_MAX/100),
													DOWNLOAD_BAR_MAX ), 1);
	if (dw->close_on_finish) {
		tree[DOWNLOAD_CB_CLOSE_RDY].ob_state |= (SELECTED | CROSSED);
	} else {
		tree[DOWNLOAD_CB_CLOSE_RDY].ob_state &= ~(SELECTED | CROSSED);
	}
	tree[DOWNLOAD_BT_ABORT].ob_state &= ~SELECTED;

	/*Walk the AES rectangle list and redraw the visible areas of the window: */
	wind_get_grect(dw->aes_handle, WF_FIRSTXYWH, &visible);
	while (visible.g_x && visible.g_y) {
		if (rc_intersect(&work, &visible)) {
			objc_draw_grect(tree, 0, 8, &visible);
		}
		wind_get_grect(dw->aes_handle, WF_NEXTXYWH, &visible);
	}
}

static void on_abort_click(struct gui_download_window *dw)
{
	if( dw->status == NSATARI_DOWNLOAD_COMPLETE
		|| dw->status == NSATARI_DOWNLOAD_ERROR ) {
			guiwin_send_msg(dw->guiwin, WM_CLOSED, 0,0,0,0);
	}
	else if( dw->status != NSATARI_DOWNLOAD_CANCELED ){
		dw->abort = true;
	}
}

static void on_cbrdy_click(struct gui_download_window *dw)
{
	dw->close_on_finish = !dw->close_on_finish;
	if (dw->close_on_finish && dw->status == NSATARI_DOWNLOAD_COMPLETE) {
		guiwin_send_msg(dw->guiwin, WM_CLOSED, 0,0,0,0);
	}
	guiwin_send_redraw(dw->guiwin, NULL);
	evnt_timer(250);
}

static void on_close(struct gui_download_window * dw)
{
	gui_download_window_destroy(dw);
}

static void gui_download_window_destroy( struct gui_download_window * gdw)
{


	LOG((""));
	if (gdw->status == NSATARI_DOWNLOAD_WORKING) {
		download_context_abort(gdw->ctx);
	}

	download_context_destroy(gdw->ctx);

	if (gdw->destination) {
		free( gdw->destination );
	}
	if (gdw->fd != NULL) {
		fclose(gdw->fd);
		gdw->fd = NULL;
	}
	if (gdw->fbuf != NULL) {
		free( gdw->fbuf );
	}
	guiwin_remove(gdw->guiwin);
	wind_close(gdw->aes_handle);
	wind_delete(gdw->aes_handle);
	free(gdw);
}

static char * select_filepath( const char * path, const char * filename )
{
	char tmp[PATH_MAX];
	char res_path[PATH_MAX];
	char res_file[PATH_MAX];
	char * ret = NULL;


	strncpy( res_path, path, PATH_MAX );
	strncpy( res_file, filename, PATH_MAX );
	res_file[PATH_MAX-1] = 0;
	res_path[PATH_MAX-1] = 0;
	if( mt_FselInput( &app, res_path, res_file, (char*)"*",
					(char*)messages_get("SaveAsNS"), res_path, NULL ) ) {
		assert( (strlen( res_path ) + strlen( res_file ) + 2) < PATH_MAX );
		snprintf(tmp, PATH_MAX, "%s%s", res_path, res_file );
		ret = malloc( strlen(tmp)+1 );
		strcpy( ret, tmp );
	}
	return( ret );
}

struct gui_download_window * gui_download_window_create(download_context *ctx,
		struct gui_window *parent)
{
	char *filename;
	char *destination;
	char gdos_path[PATH_MAX];
	const char * url;
	struct gui_download_window * gdw;
	int dlgres = 0;
	OBJECT * tree = get_tree(DOWNLOAD);


	LOG(("Creating download window for gui window: %p", parent));

	/* TODO: Implement real form and use messages file strings! */

	if (tree == NULL){
		die("Couldn't find AES Object tree for download window!");
		return(NULL);
	}

	filename = download_context_get_filename(ctx);
	dlgres = form_alert(2, "[2][Accept download?][Yes|Save as...|No]");
	if( dlgres == 3){
		return( NULL );
	}
	else if( dlgres == 2 ){
		gemdos_realpath(nsoption_charp(downloads_path), gdos_path);
		char * tmp = select_filepath( gdos_path, filename );
		if( tmp == NULL )
			return( NULL );
		destination = tmp;
	} else {
		int dstsize=0;
		gemdos_realpath(nsoption_charp(downloads_path), gdos_path);
		dstsize = strlen(gdos_path) + strlen(filename) + 2;
		destination = malloc( dstsize );
		snprintf(destination, dstsize, "%s/%s", gdos_path, filename);
	}

	gdw = calloc(1, sizeof(struct gui_download_window));
	if( gdw == NULL ){
		warn_user(NULL, "Out of memory!");
		free( destination );
		return( NULL );
	}

	gdw->ctx = ctx;
	gdw->abort = false;
	gdw->start = clock() / CLOCKS_PER_SEC;
	gdw->lastrdw = 0;
	gdw->status = NSATARI_DOWNLOAD_WORKING;
	gdw->parent = parent;
	gdw->fbufsize = MAX(BUFSIZ, 48000);
	gdw->size_downloaded = 0;
	gdw->size_total = download_context_get_total_length(ctx);
	gdw->destination = destination;
	gdw->tree = tree;
	url = download_context_get_url(ctx);

	gdw->fd = fopen(gdw->destination, "wb");
	if( gdw->fd == NULL ){
		char spare[200];
		snprintf(spare, 200, "Couldn't open %s for writing!", gdw->destination);
		msg_box_show(MSG_BOX_ALERT, spare);
		free(filename);
		gui_download_window_destroy(gdw);
		return( NULL );
	}

	gdw->fbuf = malloc( gdw->fbufsize+1 );
	if( gdw->fbuf != NULL ){
		setvbuf( gdw->fd, gdw->fbuf, _IOFBF, gdw->fbufsize );
	}

	gdw->aes_handle = wind_create_grect(CLOSER | NAME | MOVER, &desk_area);
	wind_set_str(gdw->aes_handle, WF_NAME, "Download");
	unsigned long gwflags = GW_FLAG_DEFAULTS;
	gwflags &= ~GW_FLAG_TOOLBAR_REDRAW;
	gdw->guiwin = guiwin_add(gdw->aes_handle, gwflags, on_aes_event);
	if( gdw->guiwin == NULL || gdw->fd == NULL ){
		die("could not create guiwin");
		free( filename );
		gui_download_window_destroy(gdw);
		return( NULL );
	}
	guiwin_set_user_data(gdw->guiwin, gdw);
	guiwin_set_toolbar(gdw->guiwin, tree, 0, 0);

	strncpy((char*)&gdw->lbl_file, filename, MAX_SLEN_LBL_FILE-1);
	free( filename );
	LOG(("created download: %s (total size: %d)",
		gdw->destination, gdw->size_total
	));

	GRECT work, curr;
	work.g_x = 0;
	work.g_y = 0;
	work.g_w = tree->ob_width;
	work.g_h = tree->ob_height;

	wind_calc_grect(WC_BORDER, CLOSER | MOVER | NAME, &work, &curr);

	curr.g_x = (desk_area.g_w / 2) - (curr.g_w / 2);
	curr.g_y = (desk_area.g_h / 2) - (curr.g_h / 2);

	wind_open_grect(gdw->aes_handle, &curr);
	gdw->lastrdw = clock() / (CLOCKS_PER_SEC >> 3);

	return(gdw);
}


nserror gui_download_window_data(struct gui_download_window *dw,
		const char *data, unsigned int size)
{

	uint32_t clck = clock();
	uint32_t tnow = clck / (CLOCKS_PER_SEC>>3);
	uint32_t sdiff = (clck / (CLOCKS_PER_SEC)) - dw->start;
	uint32_t p = 0;
	float speed;
	float pf = 0;

	LOG((""));

	OBJECT * tree = dw->tree;

	if(dw->abort == true){
		dw->status = NSATARI_DOWNLOAD_CANCELED;
		dw->abort = false;
		download_context_abort(dw->ctx);
		guiwin_send_redraw(dw->guiwin, NULL);
		return(NSERROR_OK);
	}

	/* save data */
	fwrite( data , size, sizeof(unsigned char),dw->fd );
	dw->size_downloaded += size;

	/* Update GUI */
	if ((tnow - dw->lastrdw) > 1) {

		dw->lastrdw = tnow;
		speed = dw->size_downloaded / sdiff;

		if( dw->size_total > 0 ){
			p = ((double)dw->size_downloaded / (double)dw->size_total * 100);
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

		guiwin_send_redraw(dw->guiwin, NULL);
	}
	return NSERROR_OK;
}

void gui_download_window_error(struct gui_download_window *dw,
                               const char *error_msg)
{
	LOG(("%s", error_msg));
	strncpy((char*)&dw->lbl_file, error_msg, MAX_SLEN_LBL_FILE-1);
	dw->status = NSATARI_DOWNLOAD_ERROR;
	guiwin_send_redraw(dw->guiwin, NULL);
	gui_window_set_status(input_window, messages_get("Done") );
	// TODO: change abort to close
}

void gui_download_window_done(struct gui_download_window *dw)
{
	OBJECT * tree;
	LOG((""));

// TODO: change abort to close
	dw->status = NSATARI_DOWNLOAD_COMPLETE;

	if( dw->fd != NULL ) {
		fclose( dw->fd );
		dw->fd = NULL;
	}

	tree = dw->tree;
	if (dw->close_on_finish) {
		guiwin_send_msg(dw->guiwin, WM_CLOSED, 0, 0, 0, 0);
	} else {
		snprintf( (char*)&dw->lbl_percent, MAX_SLEN_LBL_PERCENT,
			"%lu%s", 100, "%"
		);
		snprintf( (char*)&dw->lbl_done, MAX_SLEN_LBL_DONE, "%s / %s",
			human_friendly_bytesize(dw->size_downloaded),
			(dw->size_total>0) ? human_friendly_bytesize(dw->size_total) : human_friendly_bytesize(dw->size_downloaded)
		);
		guiwin_send_redraw(dw->guiwin, NULL);
	}
	gui_window_set_status(input_window, messages_get("Done") );
}
