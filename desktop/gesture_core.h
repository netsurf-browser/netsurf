/*
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
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

/** \file
 * Mouse gesture core (interface)
 */

#ifndef _NETSURF_DESKTOP_GESTURE_CORE_H
#define _NETSURF_DESKTOP_GESTURE_CORE_H_

#include <stdbool.h>

typedef struct _gesture_recogniser* GestureRecogniser;
typedef struct _gesturer_state* Gesturer;

GestureRecogniser gesture_recogniser_create(void);
void gesture_recogniser_add(GestureRecogniser recog,
                            const char* gesture_str, int gesture_tag);
void gesture_recogniser_destroy(GestureRecogniser recog);
void gesture_recogniser_set_distance_threshold(GestureRecogniser recog,
                                               int min_distance);
void gesture_recogniser_set_count_threshold(GestureRecogniser recog,
                                            int max_nonmove);


Gesturer gesturer_create(GestureRecogniser recog);
Gesturer gesturer_clone(Gesturer gesturer);
void gesturer_destroy(Gesturer gesturer);
int gesturer_add_point(Gesturer gesturer, int x, int y);
void gesturer_clear_points(Gesturer gesturer);

#define GESTURE_NONE -1

#endif
