/**
 * $Id: font.h,v 1.2 2002/05/11 15:22:24 bursa Exp $
 */

/**
 * structures and typedefs
 */

struct font_set;
typedef unsigned int font_id;
struct font_split {
	unsigned long width;
	unsigned long height;
	const char * end;
};

/**
 * interface
 */

struct font_set * font_set_create(void);
font_id font_add(struct font_set * font_set, const char * name, unsigned int weight,
		unsigned int size);
void font_set_free(struct font_set * font_set);
struct font_split font_split(struct font_set * font_set, font_id id, const char * text,
		unsigned long width, int force);

