/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 */

#include <math.h>
#include "oslib/os.h"
#include "netsurf/utils/config.h"
#include "netsurf/desktop/browser.h"
#include "netsurf/riscos/gui.h"
#include "netsurf/riscos/options.h"
#include "netsurf/utils/log.h"

typedef enum {
	mouseaction_NONE,
	mouseaction_BACK, mouseaction_FORWARD,
	mouseaction_RELOAD, mouseaction_PARENT,
	mouseaction_NEWWINDOW_OR_LINKFG, mouseaction_DUPLICATE_OR_LINKBG,
	mouseaction_TOGGLESIZE, mouseaction_ICONISE, mouseaction_CLOSE
} mouseaction;


static double calculate_angle(double x, double y);
static int anglesDifferent(double a, double b);
static mouseaction ro_gui_try_mouse_action(void);


void ro_gui_mouse_action(gui_window *g) {

  int x, y;
  mouseaction ma = mouseaction_NONE;

  if (option_use_mouse_gestures)
    ma = ro_gui_try_mouse_action();

  if (ma == mouseaction_NONE) {

    os_mouse(&x, &y, NULL, NULL);
    ro_gui_create_menu(browser_menu, x - 64, y, g);
  }
  else {

    LOG(("MOUSE GESTURE %d", ma));
    switch (ma) {

      case mouseaction_BACK:
/*            browser_window_back(g->data.browser.bw); */
           break;

      case mouseaction_FORWARD:
/*            browser_window_forward(g->data.browser.bw); */
           break;

      case mouseaction_RELOAD:
/*           browser_window_open_location_historical(g->data.browser.bw,
           		g->data.browser.bw->url
#ifdef WITH_POST
           		, 0, 0
#endif
           		);*/
           break;

      default: break;
    }
  }
}

double calculate_angle(double x, double y) {

  double a;

  if (x == 0.0) {

    if (y < 0.0)
      a = 0.0;
    else
      a = M_PI;
  }
  else {

    a = atan(y / x);

    if (x > 0.0)
      a += M_PI_2;
    else
      a -= M_PI_2;
  }

  return a;
}

int anglesDifferent(double a, double b) {

  double c;

  if (a < 0.0)
    a += M_2_PI;

  if (b < 0.0)
    b += M_2_PI;

  if (a > M_2_PI)
    a -= M_2_PI;

  if (b > M_2_PI)
    b -= M_2_PI;

  c = a - b;

  if (c < 0.0)
    c += M_2_PI;

  if (c > M_2_PI)
    c -= M_2_PI;

  return (c > M_PI / 6.0);
}

#define STOPPED 2
#define THRESHOLD 16
#define DAMPING 1

/* TODO - something in the following function causes a segfault when
 *        mouse actions are turned on. Obviously this needs fixing.
 */

mouseaction ro_gui_try_mouse_action(void) {

  os_coord start, current, last, offset, moved;
  double offsetDistance, movedDistance;
  double angle, oldAngle=0.0;
  bits z;
  os_t now;
  int status;
  int m;
  enum {move_NONE, move_LEFT, move_RIGHT, move_UP, move_DOWN} moves[5];

  moves[0] = move_NONE;
  m = 1;

  os_mouse(&start.x, &start.y, &z, &now);
  status = 0;

  do {

    os_mouse(&current.x, &current.y, &z, &now);
    offset.x = current.x - start.x;
    offset.y = current.y - start.y;
    moved.x = current.x - last.x;
    moved.y = current.y - last.y;
    offsetDistance = sqrt((float)(offset.x * offset.x + offset.y * offset.y));
    if (moved.x > 0 || moved.y > 0)
      movedDistance = sqrt((float)(moved.x * moved.x + moved.y * moved.y));
    else
      movedDistance = 0.0;

    angle = calculate_angle((float)offset.x, (float)offset.y);

    switch (status) {

      case 1:
        if (movedDistance < STOPPED ||
         (movedDistance > STOPPED*2.0 && anglesDifferent(angle, oldAngle))) {

          start.x = current.x;
          start.y = current.y;
          status = 0;
        }
        break;

      case 0:
        if (offsetDistance > THRESHOLD) {

          if (fabs((float)offset.x) > fabs((float)offset.y)) {

            if (fabs((float)offset.y) < fabs((float)offset.x) * DAMPING &&
                fabs((float)offset.x) > THRESHOLD*0.75) {

              if (offset.x < 0)
                moves[m] = move_LEFT;
              else
                moves[m] = move_RIGHT;

              if (moves[m] != moves[m-1])
                m++;

              start.x = current.x;
              start.y = current.y;
              oldAngle = angle;
              status = 1;
            }
          }
          else if (fabs((float)offset.y) > fabs((float)offset.x)) {

            if (fabs((float)offset.x) < fabs((float)offset.y) * DAMPING &&
                fabs((float)offset.y) > THRESHOLD*0.75) {

              if (offset.y < 0)
                moves[m] = move_DOWN;
              else
                moves[m] = move_UP;

              if (moves[m] != moves[m-1])
                m++;

              start.x = current.x;
              start.y = current.y;
              oldAngle = angle;
              status = 1;
            }
          }
        }
        break;
    }

    last.x = current.x;
    last.y = current.y;

  } while ((z & 2) != 0 && m < 4);

  LOG(("MOUSEACTIONS: %d %d %d %d\n", moves[0], moves[1],
                                      moves[2], moves[3]));
  if (m == 2) {

    switch (moves[1]) {

      case move_LEFT:
        LOG(("mouse action: go back"));
        return mouseaction_BACK;

      case move_RIGHT:
        LOG(("MOUSE ACTION: GO FORWARD"));
        return mouseaction_FORWARD;

      case move_DOWN:
        LOG(("mouse action: create new window // open link in new window, foreground"));
        return mouseaction_NEWWINDOW_OR_LINKFG;

      default: break;
    }
  }

  if (m == 3) {

    switch (moves[1]) {

      case move_UP:
        switch (moves[2]) {

          case move_DOWN:
            LOG(("mouse action: reload"));
            return mouseaction_RELOAD;

          case move_RIGHT:
            LOG(("mouse action: toggle size"));
            return mouseaction_TOGGLESIZE;

          case move_LEFT:
            LOG(("mouse action: parent directroy"));
            return mouseaction_PARENT;

          default: break;
        }
        break;

      case move_DOWN:
        switch (moves[2]) {

          case move_LEFT:
            LOG(("mouse action: iconise"));
            return mouseaction_ICONISE;

          case move_UP:
            LOG(("mouse action: duplicate // open link in new window, background"));
            return mouseaction_DUPLICATE_OR_LINKBG;

          case move_RIGHT:
            LOG(("mouse action: close"));
            return mouseaction_CLOSE;

          default: break;
        }
        break;

      default: break;
    }
  }

  if (m == 4) {

    if (moves[1] == move_RIGHT && moves[2] == move_LEFT &&
        moves[3] == move_RIGHT) {

      LOG(("mouse action: close window"));
      return mouseaction_CLOSE;
    }
  }

  return mouseaction_NONE;
}
