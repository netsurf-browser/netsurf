/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
 */

/** \file
 * Mouse gesture core (implementation)
 */

#include "netsurf/utils/log.h"
#include "netsurf/desktop/gesture_core.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

/** A gesture as used by the recognition machinery */
struct _internal_gesture {
	struct _internal_gesture *next; /**< The next gesture in the list */
	int gesture_tag; /**< The tag to return for this gesture */
	int gesture_len; /**< The length of this gesture string */
	char *gesture; /**< The gesture string reversed for matching */
};

typedef struct _internal_gesture* InternalGesture;

/** A recogniser state. Commonly one in the application. Could have
 * multiple (E.g. one for browser windows, one for the history window.
 */
struct _gesture_recogniser {
	InternalGesture gestures; /**< The gestures registered */
	Gesturer gesture_users; /**< The users of the gesture engine */
	int max_len; /**< The maximum length the gestures in this recogniser */
	int min_distance; /**< The minimum distance the mouse should move */
	int max_nonmove; /**< The maximum number of data points before abort */
};

/** A gesturer state. Commonly one per browser window */
struct _gesturer_state {
	GestureRecogniser recogniser; /**< The recogniser for this state */
	Gesturer next; /* Next gesture state */
	int last_x; /**< Last X coordinate fed to the gesture engine */
	int last_y; /**< Last Y coordinate fed to the gesture engine */
	int bored_count; /**< Num of boring recent add_point calls */
	int elements; /**< Number of elements in the current gesture */
	int nelements; /**< The max number of elements in this gesturer */
	char *gesture; /**< The in-progress gesture string */
};

static void gesturer_notify_recognition_change(Gesturer gesturer);

/** Create a gesture recogniser.
 *
 * \return A new recogniser.
 */
GestureRecogniser gesture_recogniser_create(void)
{
	GestureRecogniser ret = malloc(sizeof(struct _gesture_recogniser));
	ret->gestures = NULL;
	ret->gesture_users = NULL;
	ret->max_len = 0;
	ret->min_distance = 1000000; /* Extremely unlikely */
	ret->max_nonmove = 1;
	return ret;
}

/** Add a gesture to the recogniser.
 *
 * \param recog The recogniser
 * \param gesture_str The gesture string to add
 * \param gesture_tag The tag to return for this gesture
 */
void gesture_recogniser_add(GestureRecogniser recog,
			    const char* gesture_str, int gesture_tag)
{
	InternalGesture g = malloc(sizeof(struct _internal_gesture));
	InternalGesture g2,g3;
	int i;
	Gesturer gest = recog->gesture_users;

	g->gesture_tag = gesture_tag;
	g->gesture_len = strlen(gesture_str);
	g->gesture = malloc(g->gesture_len);

	for(i = 0; i < g->gesture_len; ++i)
		g->gesture[i] = gesture_str[g->gesture_len - i - 1];

	g2 = recog->gestures; g3 = NULL;
	while( g2 && g->gesture_len < g2->gesture_len ) g3=g3, g2 = g2->next;
	if( g3 == NULL ) {
		/* prev == NULL, this means we're inserting at the head */
		recog->gestures = g; g->next = g2;
	} else {
		/* prev == something; we're inserting somewhere	 */
		g3->next = g; g->next = g2;
	}

	if( recog->max_len < g->gesture_len ) recog->max_len = g->gesture_len;

	while( gest != NULL ) {
		gesturer_notify_recognition_change(gest);
		gest = gest->next;
	}
}

/** Destroy a gesture recogniser.
 *
 * Only call this after destroying all the gesturers for it.
 *
 * \param recog The recogniser to destroy.
 */
void gesture_recogniser_destroy(GestureRecogniser recog)
{
	if( recog->gesture_users ) {
		LOG(("Attempt to destroy a gesture recogniser with gesture users still registered."));
		return;
	}
	while( recog->gestures ) {
		InternalGesture g = recog->gestures;
		recog->gestures = g->next;
		free(g->gesture);
		free(g);
	}
	free(recog);
	return;
}

/** Set the min distance the mouse has to move in order to be
 * classed as having partaken of a gesture.
 *
 * \param recog The recogniser.
 * \param min_distance The minimum distance in pixels
 */
void gesture_recogniser_set_distance_threshold(GestureRecogniser recog,
					       int min_distance)
{
	recog->min_distance = min_distance * min_distance;
}

/** Set the number of non-movement adds of points before the gesturer is
 * internally reset instead of continuing to accumulate a gesture.
 *
 * \param recog The recogniser.
 * \param max_nonmove The maximum number of non-movement adds.
 */
void gesture_recogniser_set_count_threshold(GestureRecogniser recog,
					    int max_nonmove)
{
	recog->max_nonmove = max_nonmove;
}

/** Create a gesturer.
 *
 * \param recog The gesture recogniser for this gesturer.
 *
 * \return The new gesturer object
 */
Gesturer gesturer_create(GestureRecogniser recog)
{
	Gesturer ret = malloc(sizeof(struct _gesturer_state));
	ret->recogniser = recog;
	ret->next = recog->gesture_users;
	recog->gesture_users = ret;
	ret->last_x = 0;
	ret->last_y = 0;
	ret->bored_count = 0;
	ret->elements = 0;
	ret->nelements = recog->max_len;
	ret->gesture = calloc(recog->max_len+1, 1);
	return ret;
}

/** Clone a gesturer.
 *
 * \param gesturer The gesturer to clone
 *
 * \return A gesturer cloned from the parameter
 */
Gesturer gesturer_clone(Gesturer gesturer)
{
	return gesturer_create(gesturer->recogniser);
}

/** Remove this gesturer from its recogniser and destroy it.
 *
 * \param gesturer The gesturer to destroy.
 */
void gesturer_destroy(Gesturer gesturer)
{
	Gesturer g = gesturer->recogniser->gesture_users, g2 = NULL;
	while( g && g != gesturer ) g2 = g, g = g->next;
	if( g2 == NULL ) {
		/* This gesturer is first in the list */
		gesturer->recogniser->gesture_users = gesturer->next;
	} else {
		g2->next = gesturer->next;
	}
	free(gesturer->gesture);
	free(gesturer);
}

/** Notify a gesturer that its recogniser has changed in some way */
static void gesturer_notify_recognition_change(Gesturer gesturer)
{
	char *new_gesture = calloc(gesturer->recogniser->max_len+1, 1);
	int i;
	for(i = 0; i < gesturer->elements; ++i)
		new_gesture[i] = gesturer->gesture[i];
	free(gesturer->gesture);
	gesturer->gesture = new_gesture;
	gesturer->nelements = gesturer->recogniser->max_len;
}

/** Clear the points associated with this gesturer.
 *
 * You might call this if the gesturer should be cleared because a mouse
 * button was released or similar.
 *
 * \param gesturer The gesturer to clear.
 */
void gesturer_clear_points(Gesturer gesturer)
{
	memset(gesturer->gesture, 0, gesturer->elements);
	gesturer->elements = 0;
	gesturer->bored_count = 0;
}

#define M_PI_8 (M_PI_4 / 2)
#define M_3_PI_8 (3 * M_PI_8)

static struct {
	float lower, upper;
	bool x_neg, y_neg;
	char direction;
} directions[12] = {
	/* MIN	    MAX	      X_NEG  Y_NEG  DIRECTION */
	{ 0.0,	    M_PI_8,   false, false, '1' }, /* Right */
	{ M_PI_8,   M_3_PI_8, false, false, '2' }, /* Up/Right */
	{ M_3_PI_8, INFINITY, false, false, '3' }, /* Up */
	{ M_3_PI_8, INFINITY, true,  false, '3' }, /* Up */
	{ M_PI_8,   M_3_PI_8, true,  false, '4' }, /* Up/Left */
	{ 0.0,	    M_PI_8,   true,  false, '5' }, /* Left */
	{ 0.0,	    M_PI_8,   true,  true,  '5' }, /* Left */
	{ M_PI_8,   M_3_PI_8, true,  true,  '6' }, /* Down/Left */
	{ M_3_PI_8, INFINITY, true,  true,  '7' }, /* Down */
	{ M_3_PI_8, INFINITY, false, true,  '7' }, /* Down */
	{ M_PI_8,   M_3_PI_8, false, true,  '8' }, /* Down/Right */
	{ 0.0,	    M_PI_8,   false, true,  '1' }  /* Right */
};

#undef M_PI_8
#undef M_3_PI_8

static char gesturer_find_direction(Gesturer gesturer, int x, int y)
{
	float dx = 0.0000000000001 + (x - gesturer->last_x);
	float dy = -(0.0000000000001 + (y - gesturer->last_y));
	float arc;
	bool x_neg = dx < 0;
	bool y_neg = dy < 0;
	int i;

	if( x_neg ) dx = -dx;
	if( y_neg ) dy = -dy;
	arc = atanf(dy/dx);
	for( i = 0; i < 12; ++i ) {
		if( directions[i].lower > arc || directions[i].upper <= arc )
			continue; /* Not within this entry */
		if( directions[i].x_neg != x_neg ||
		    directions[i].y_neg != y_neg )
			continue; /* Signs not matching */
		return directions[i].direction;
	}
	LOG(("Erm, fell off the end of the direction calculator"));
	return 0; /* No direction */
}

/** Indicate to a gesturer that a new mouse sample is available.
 *
 * Call this to provide a new position sample to the gesturer.
 * If this is interesting, the gesturer will return a gesture tag
 * as per the gesture recogniser it was constructed with. Otherwise
 * it will return GESTURE_NONE which has the value -1.
 *
 * \param gesturer The gesturer to add the point to.
 * \param x The X coordinate of the mouse pointer.
 * \param y The Y coordinate of the mouse pointer.
 *
 * \return The gesture tag activated (or GESTURE_NONE if none)
 */
int gesturer_add_point(Gesturer gesturer, int x, int y)
{
	int distance = ((gesturer->last_x - x) * (gesturer->last_x - x)) +
		((gesturer->last_y - y) * (gesturer->last_y - y));
	char last_direction = (gesturer->elements == 0) ? 0 :
		(gesturer->gesture[0]);
	char this_direction = gesturer_find_direction(gesturer, x, y);
	InternalGesture ig = gesturer->recogniser->gestures;
	int ret = GESTURE_NONE;

	if( distance < gesturer->recogniser->min_distance ) {
		gesturer->bored_count++;
		if( gesturer->elements &&
		    (gesturer->bored_count >=
		     gesturer->recogniser->max_nonmove) ) {
			LOG(("Bored now."));
			gesturer_clear_points(gesturer);
		}
		if( gesturer->elements &&
		    gesturer->bored_count ==
		    (gesturer->recogniser->max_nonmove >> 1)) {
			LOG(("Decided to look"));
			while( ig && ig->gesture_len <= gesturer->elements ) {
				if( strcmp(gesturer->gesture,
					   ig->gesture) == 0 )
					ret = ig->gesture_tag;
				ig = ig->next;
			}
		}
		return ret; /* GESTURE_NONE or else a gesture found above */
	}
	/* We moved far enough that we care about the movement */
	gesturer->last_x = x;
	gesturer->last_y = y;
	gesturer->bored_count = 0;
	if( this_direction == last_direction ) {
		return GESTURE_NONE; /* Nothing */
	}

	/* Shunt the gesture one up */
	if( gesturer->elements ) {
		if( gesturer->elements == gesturer->nelements )
			gesturer->elements--;
		memmove(gesturer->gesture+1, gesturer->gesture,
			gesturer->elements);
	}
	gesturer->elements++;
	gesturer->gesture[0] = this_direction;
	LOG(("Gesture is currently: '%s'", gesturer->gesture));
	return GESTURE_NONE;
}
