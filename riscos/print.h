/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#ifndef _NETSURF_RISCOS_PRINT_H_
#define _NETSURF_RISCOS_PRINT_H_

struct gui_window;

extern struct gui_window *print_current_window;

void print_save_bounce(wimp_message *m);
void print_error(wimp_message *m);
void print_type_odd(wimp_message *m);
bool print_ack(wimp_message *m);
void print_dataload_bounce(wimp_message *m);
void print_cleanup(void);

#endif

