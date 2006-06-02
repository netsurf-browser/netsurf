/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
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
