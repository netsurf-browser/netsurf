/**
 * $Id: font.c,v 1.1 2002/04/25 15:52:26 bursa Exp $
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "font.h"

/**
 * internal structures
 */

struct font_set {
	/* a set of font handles */
};

/**
 * functions
 */

struct font_set * font_set_create(void)
{
	return 0;
}
	
font_id font_add(struct font_set * font_set, const char * name, unsigned int weight,
		unsigned int size)
{
	return 0;
}

void font_set_free(struct font_set * font_set)
{
}

/**
 * find where to split some text to fit it in width
 */

unsigned long font_split(struct font_set * font_set, font_id id, const char * text,
		unsigned long width, const char ** end)
{
	size_t len = strlen(text);
	unsigned int i;
	assert(width >= 1);
	if (len <= width) {
		*end = text + len;
		return len;
	}
	/* invariant: no space in text[i+1..width) */
	for (i = width - 1; i != 0 && text[i] != ' '; i--)
		;
	*end = text + i;
	return i;
}


