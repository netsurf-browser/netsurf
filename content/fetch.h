/**
 * $Id: fetch.h,v 1.2 2003/03/15 15:53:20 bursa Exp $
 */

#ifndef _NETSURF_DESKTOP_FETCH_H_
#define _NETSURF_DESKTOP_FETCH_H_

typedef enum {FETCH_TYPE, FETCH_DATA, FETCH_FINISHED, FETCH_ERROR} fetch_msg;

struct content;
struct fetch;

void fetch_init(void);
struct fetch * fetch_start(char *url, char *referer,
                 void (*callback)(fetch_msg msg, void *p, char *data, unsigned long size), void *p);
void fetch_abort(struct fetch *f);
void fetch_poll(void);
void fetch_quit(void);
const char *fetch_filetype(const char *unix_path);

#endif
