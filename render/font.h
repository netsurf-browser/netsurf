/**
 * $Id: font.h,v 1.1 2002/04/25 15:52:26 bursa Exp $
 */

/**
 * structures and typedefs
 */

struct font_set;
typedef unsigned int font_id;

/**
 * interface
 */

struct font_set * font_set_create(void);
font_id font_add(struct font_set * font_set, const char * name, unsigned int weight,
		unsigned int size);
void font_set_free(struct font_set * font_set);
unsigned long font_split(struct font_set * font_set, font_id id, const char * text,
		unsigned long width, const char ** end);

