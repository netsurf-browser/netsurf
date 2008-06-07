/*
 * Copyright 2008 François Revol <mmu_man@users.sourceforge.net>
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
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

#ifndef NETSURF_BEOS_SCAFFOLDING_H
#define NETSURF_BEOS_SCAFFOLDING_H 1

#include <View.h>
#include <Window.h>
extern "C" {
#include "desktop/gui.h"
#include "desktop/plotters.h"
}

typedef struct beos_scaffolding nsbeos_scaffolding;

class NSBrowserWindow : public BWindow {
public:
		NSBrowserWindow(BRect frame, struct beos_scaffolding *scaf);
virtual	~NSBrowserWindow();

virtual void	MessageReceived(BMessage *message);
virtual bool	QuitRequested(void);

struct beos_scaffolding *Scaffolding() const { return fScaffolding; };

private:
	struct beos_scaffolding *fScaffolding;
};


NSBrowserWindow *nsbeos_find_last_window(void);

nsbeos_scaffolding *nsbeos_new_scaffolding(struct gui_window *toplevel);

bool nsbeos_scaffolding_is_busy(nsbeos_scaffolding *scaffold);

void nsbeos_attach_toplevel_view(nsbeos_scaffolding *g, BView *view);

#if 0 /* GTK */
void nsbeos_attach_toplevel_viewport(nsbeos_scaffolding *g, GtkViewport *vp);
#endif

void nsbeos_scaffolding_dispatch_event(nsbeos_scaffolding *scaffold, BMessage *message);

void nsbeos_scaffolding_destroy(nsbeos_scaffolding *scaffold);

//void nsbeos_window_destroy_event(NSBrowserWindow *window, nsbeos_scaffolding *g, BMessage *event);


void nsbeos_scaffolding_popup_menu(nsbeos_scaffolding *g, BPoint where);

#if 0 /* GTK */
void nsbeos_scaffolding_popup_menu(nsbeos_scaffolding *g, guint button);
#endif

#endif /* NETSURF_BEOS_SCAFFOLDING_H */
