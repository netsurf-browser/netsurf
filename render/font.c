/**
 * $Id: font.c,v 1.2 2002/05/11 15:22:24 bursa Exp $
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

struct font_split font_split(struct font_set * font_set, font_id id, const char * text,
		unsigned long width, int force)
{
	size_t len = strlen(text);
	unsigned int i;
	struct font_split split;

	split.height = 30;
	
	if (len * 20 <= width) {
		split.width = len * 20;
		split.end = text + len;
	} else {
		for (i = width / 20; i != 0 && text[i] != ' '; i--)
			;
		if (force && i == 0) {
			i = width / 20;
			if (i == 0) i = 1;
		}
		split.width = i * 20;
		if (text[i] == ' ') i++;
		split.end = text + i;
	}

	return split;
}


