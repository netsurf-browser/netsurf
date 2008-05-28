/*
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
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

/*
 * Much of this shamelessly copied from utils/messages.c
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "content/content.h"
#include "render/box.h"
#include "render/imagemap.h"
#include "utils/log.h"
#include "utils/utils.h"

#define HASH_SIZE 31 /* fixed size hash table */

typedef enum {
	IMAGEMAP_DEFAULT,
	IMAGEMAP_RECT,
	IMAGEMAP_CIRCLE,
	IMAGEMAP_POLY
} imagemap_entry_type;

struct mapentry {
	imagemap_entry_type type;	/**< type of shape */
	char *url;			/**< absolute url to go to */
	char *target;			/**< target frame (if any) */
	union {
		struct {
			int x;		/**< x coordinate of centre */
			int y;		/**< y coordinate of center */
			int r;		/**< radius of circle */
		} circle;
		struct {
			int x0;		/**< left hand edge */
			int y0;		/**< top edge */
			int x1;		/**< right hand edge */
			int y1;		/**< bottom edge */
		} rect;
		struct {
			int num;	/**< number of points */
			float *xcoords;	/**< x coordinates */
			float *ycoords;	/**< y coordinates */
		} poly;
	} bounds;
	struct mapentry *next;		/**< next entry in list */
};

struct imagemap {
	char *key;		/**< key for this entry */
	struct mapentry *list;	/**< pointer to linked list of entries */
	struct imagemap *next;	/**< next entry in this hash chain */
};

static bool imagemap_add(struct content *c, const char *key,
		struct mapentry *list);
static bool imagemap_create(struct content *c);
static bool imagemap_extract_map(xmlNode *node, struct content *c,
		struct mapentry **entry);
static bool imagemap_addtolist(xmlNode *n, char *base_url,
		struct mapentry **entry);
static void imagemap_freelist(struct mapentry *list);
static unsigned int imagemap_hash(const char *key);
static int imagemap_point_in_poly(int num, float *xpt, float *ypt,
		unsigned long x, unsigned long y, unsigned long click_x,
		unsigned long click_y);

/**
 * Add an imagemap to the hashtable, creating it if it doesn't exist
 *
 * \param c The containing content
 * \param key The name of the imagemap
 * \param list List of map regions
 * \return true on succes, false otherwise
 */
bool imagemap_add(struct content *c, const char *key, struct mapentry *list)
{
	struct imagemap *map;
	unsigned int slot;

	assert(c != NULL);
	assert(c->type == CONTENT_HTML);
	assert(key != NULL);
	assert(list != NULL);

	imagemap_create(c);

	map = calloc(1, sizeof(*map));
	if (!map) {
		return false;
	}
	map->key = strdup(key);
	if (!map->key) {
		free(map);
		return false;
	}
	map->list = list;
	slot = imagemap_hash(key);
	map->next = c->data.html.imagemaps[slot];
	c->data.html.imagemaps[slot] = map;

	return true;
}

/**
 * Create hashtable of imagemaps
 *
 * \param c The containing content
 * \return true on success, false otherwise
 */
bool imagemap_create(struct content *c)
{
	assert(c != NULL);
	assert(c->type == CONTENT_HTML);

	if (c->data.html.imagemaps == 0) {
		c->data.html.imagemaps = calloc(HASH_SIZE,
						 sizeof(struct imagemap));
		if (!c->data.html.imagemaps) {
			return false;
		}
	}

	return true;
}

/**
 * Destroy hashtable of imagemaps
 *
 * \param c The containing content
 */
void imagemap_destroy(struct content *c)
{
	unsigned int i;

	assert(c != NULL);
	assert(c->type == CONTENT_HTML);

	/* no imagemaps -> return */
	if (c->data.html.imagemaps == 0) return;

	for (i = 0; i != HASH_SIZE; i++) {
		struct imagemap *map, *next;
		map = c->data.html.imagemaps[i];
		while (map != 0) {
			next = map->next;
			imagemap_freelist(map->list);
			free(map->key);
			free(map);
			map = next;
		}
	}

	free(c->data.html.imagemaps);
}

/**
 * Dump imagemap data to the log
 *
 * \param c The containing content
 */
void imagemap_dump(struct content *c)
{
	unsigned int i;

	int j;

	assert(c != NULL);
	assert(c->type == CONTENT_HTML);

	if (c->data.html.imagemaps == 0) return;

	for (i = 0; i != HASH_SIZE; i++) {
		struct imagemap *map;
		struct mapentry *entry;
		map = c->data.html.imagemaps[i];
		while (map != 0) {
			LOG(("Imagemap: %s", map->key));
			for (entry = map->list; entry; entry = entry->next) {
				switch (entry->type) {
				case IMAGEMAP_DEFAULT:
					LOG(("\tDefault: %s", entry->url));
					break;
				case IMAGEMAP_RECT:
					LOG(("\tRectangle: %s: [(%d,%d),(%d,%d)]",
						entry->url,
						entry->bounds.rect.x0,
						entry->bounds.rect.y0,
						entry->bounds.rect.x1,
						entry->bounds.rect.y1));
					break;
				case IMAGEMAP_CIRCLE:
					LOG(("\tCircle: %s: [(%d,%d),%d]",
						entry->url,
						entry->bounds.circle.x,
						entry->bounds.circle.y,
						entry->bounds.circle.r));
					break;
				case IMAGEMAP_POLY:
					LOG(("\tPolygon: %s:", entry->url));
					for (j=0; j!=entry->bounds.poly.num;
							j++) {
						fprintf(stderr, "(%d,%d) ",
							(int)entry->bounds.poly.xcoords[j],
							(int)entry->bounds.poly.ycoords[j]);
					}
					fprintf(stderr,"\n");
					break;
				}
			}
			map = map->next;
		}
	}
}

/**
 * Extract all imagemaps from a document tree
 *
 * \param node Root node of tree
 * \param c The containing content
 * \return false on memory exhaustion, true otherwise
 */
bool imagemap_extract(xmlNode *node, struct content *c)
{
	xmlNode *this_node;
	struct mapentry *entry = 0;
	char *name;

	assert(node != NULL);
	assert(c != NULL);

	if (node->type == XML_ELEMENT_NODE) {
		if (strcmp((const char *) node->name, "map") == 0) {
			if ((name = (char *) xmlGetProp(node,
					(const xmlChar *) "id")) == NULL) {
				if ((name = (char *) xmlGetProp(node,
					(const xmlChar *) "name")) ==
						NULL)
					return true;
			}
			if (!imagemap_extract_map(node, c, &entry)) {
				xmlFree(name);
				return false;
			}
			/* imagemap_extract_map may not extract anything,
			 * so entry can still be NULL here. This isn't an
			 * error as it just means that we've encountered
			 * an incorrectly defined <map>...</map> block
			 */
			if (entry) {
				if (!imagemap_add(c, name, entry)) {
					xmlFree(name);
					return false;
				}
			}
			xmlFree(name);
			return true;
		}
	}
	else return true;

	/* now recurse */
	for (this_node = node->children; this_node != 0;
					this_node = this_node->next) {
		if (!imagemap_extract(this_node, c))
			return false;
	}

	return true;
}

/**
 * Extract an imagemap from html source
 *
 * \param node  XML node containing map
 * \param c     Content containing document
 * \param entry List of map entries
 * \return false on memory exhaustion, true otherwise
 */
bool imagemap_extract_map(xmlNode *node, struct content *c,
		struct mapentry **entry)
{
	xmlNode *this_node;

	assert(c != NULL);
	assert(entry != NULL);

	if (node->type == XML_ELEMENT_NODE) {
		/** \todo ignore <area> elements if there are other
		 *	block-level elements present in map
		 */
		if (strcmp((const char *) node->name, "area") == 0 ||
		    strcmp((const char *) node->name, "a") == 0) {
			if (!imagemap_addtolist(node,
					c->data.html.base_url, entry))
				return false;
		}
	} else {
		return true;
	}

	for (this_node = node->children; this_node != 0;
					this_node = this_node->next) {
		if (!imagemap_extract_map(this_node, c, entry))
			return false;
	}

	return true;
}

/**
 * Adds an imagemap entry to the list
 *
 * \param n     The xmlNode representing the entry to add
 * \param base_url  Base URL for resolving relative URLs
 * \param entry Pointer to list of entries
 * \return false on memory exhaustion, true otherwise
 */
bool imagemap_addtolist(xmlNode *n, char *base_url, struct mapentry **entry)
{
	char *shape, *coords = 0, *href, *val, *target = 0;
	int num;
	struct mapentry *new_map, *temp;

	assert(n != NULL);
	assert(base_url != NULL);
	assert(entry != NULL);

	if (strcmp((const char *) n->name, "area") == 0) {
		/* nohref attribute present - ignore this entry */
		if (xmlGetProp(n, (const xmlChar*)"nohref") != 0) {
			return true;
		}
	}
	/* no href -> ignore */
	if ((href = (char*)xmlGetProp(n, (const xmlChar*)"href")) == NULL) {
		return true;
	}

	target = (char *)xmlGetProp(n, (const xmlChar *)"target");

	/* no shape -> shape is a rectangle */
	if ((shape = (char*)xmlGetProp(n, (const xmlChar*)"shape")) == NULL) {
		shape = (char*)xmlMemStrdup("rect");
	}
	if (strcasecmp(shape, "default") != 0) {
		/* no coords -> ignore */
		if ((coords = (char*)xmlGetProp(n, (const xmlChar*)"coords")) == NULL) {
			if (target)
				xmlFree(target);
			xmlFree(href);
			xmlFree(shape);
			return true;
		}
	}

	new_map = calloc(1, sizeof(*new_map));
	if (!new_map) {
		if (target)
			xmlFree(target);
		xmlFree(href);
		xmlFree(shape);
		if (coords)
			xmlFree(coords);
		return false;
	}

	/* extract area shape */
	if (strcasecmp(shape, "rect") == 0) {
		new_map->type = IMAGEMAP_RECT;
	}
	else if (strcasecmp(shape, "circle") == 0) {
		new_map->type = IMAGEMAP_CIRCLE;
	}
	else if (strcasecmp(shape, "poly") == 0 ||
		strcasecmp(shape, "polygon") == 0) {
		/* polygon shape name is not valid but sites use it */
		new_map->type = IMAGEMAP_POLY;
	}
	else if (strcasecmp(shape, "default") == 0) {
		new_map->type = IMAGEMAP_DEFAULT;
	}
	else { /* unknown shape -> bail */
		free(new_map);
		if (target)
			xmlFree(target);
		xmlFree(href);
		xmlFree(shape);
		if (coords)
			xmlFree(coords);
		return true;
	}

	if (!box_extract_link(href, base_url, &new_map->url)) {
		free(new_map);
		if (target)
			xmlFree(target);
		xmlFree(href);
		xmlFree(shape);
		if (coords)
			xmlFree(coords);
		return false;
	}

	if (!new_map->url) {
		/* non-fatal error -> ignore this entry */
		free(new_map);
		if (target)
			xmlFree(target);
		xmlFree(href);
		xmlFree(shape);
		if (coords)
			xmlFree(coords);
		return true;
	}

	if (target) {
		new_map->target = strdup(target);
		if (!new_map->target) {
			free(new_map->url);
			free(new_map);
			xmlFree(target);
			xmlFree(href);
			xmlFree(shape);
			if (coords)
				xmlFree(coords);
			return false;
		}

		/* no longer needed */
		xmlFree(target);
	}

	if (new_map->type != IMAGEMAP_DEFAULT) {
		/* coordinates are a comma-separated list of values */
		val = strtok(coords, ",");
		num = 1;

		switch (new_map->type) {
		case IMAGEMAP_RECT:
			/* (left, top, right, bottom) */
			while (val && num <= 4) {
				switch (num) {
				case 1:
					new_map->bounds.rect.x0 = atoi(val);
					break;
				case 2:
					new_map->bounds.rect.y0 = atoi(val);
					break;
				case 3:
					new_map->bounds.rect.x1 = atoi(val);
					break;
				case 4:
					new_map->bounds.rect.y1 = atoi(val);
					break;
				}
				num++;
				val = strtok('\0', ",");
			}
			break;
		case IMAGEMAP_CIRCLE:
			/* (x, y, radius ) */
			while (val && num <= 3) {
				switch (num) {
				case 1:
					new_map->bounds.circle.x = atoi(val);
					break;
				case 2:
					new_map->bounds.circle.y = atoi(val);
					break;
				case 3:
					new_map->bounds.circle.r = atoi(val);
					break;
				}
				num++;
				val = strtok('\0', ",");
			}
			break;
		case IMAGEMAP_POLY:
			new_map->bounds.poly.xcoords =
					calloc(0, sizeof(*new_map->bounds.poly.xcoords));
			if (!new_map->bounds.poly.xcoords) {
				free(new_map->target);
				free(new_map->url);
				free(new_map);
				xmlFree(href);
				xmlFree(shape);
				if (coords)
					xmlFree(coords);
				return false;
			}
			new_map->bounds.poly.ycoords =
					 calloc(0, sizeof(*new_map->bounds.poly.ycoords));
			if (!new_map->bounds.poly.ycoords) {
				free(new_map->bounds.poly.xcoords);
				free(new_map->target);
				free(new_map->url);
				free(new_map);
				xmlFree(href);
				xmlFree(shape);
				xmlFree(coords);
				return false;
			}
			int x, y;
			float *xcoords, *ycoords;
			while (val) {
				x = atoi(val);
				val = strtok('\0', ",");
				if (!val)
					break;
				y = atoi(val);

				xcoords = realloc(new_map->bounds.poly.xcoords,
					num * sizeof(*new_map->bounds.poly.xcoords));
				if (!xcoords) {
					free(new_map->bounds.poly.ycoords);
					free(new_map->bounds.poly.xcoords);
					free(new_map->target);
					free(new_map->url);
					free(new_map);
					xmlFree(href);
					xmlFree(shape);
					xmlFree(coords);
					return false;
				}
				ycoords = realloc(new_map->bounds.poly.ycoords,
					num * sizeof(*new_map->bounds.poly.ycoords));
				if (!ycoords) {
					free(new_map->bounds.poly.ycoords);
					free(new_map->bounds.poly.xcoords);
					free(new_map->target);
					free(new_map->url);
					free(new_map);
					xmlFree(href);
					xmlFree(shape);
					xmlFree(coords);
					return false;
				}

				new_map->bounds.poly.xcoords = xcoords;
				new_map->bounds.poly.ycoords = ycoords;

				new_map->bounds.poly.xcoords[num-1] = x;
				new_map->bounds.poly.ycoords[num-1] = y;

				num++;
				val = strtok('\0', ",");
			}

			new_map->bounds.poly.num = num-1;

			break;
		default:
			break;
		}
	}

	new_map->next = 0;
	if (entry && (*entry)) {
		/* add to END of list */
		for (temp = (*entry); temp->next != 0; temp = temp->next)
			;
		temp->next = new_map;
	}
	else {
		(*entry) = new_map;
	}

	xmlFree(href);
	xmlFree(shape);
	if (coords)
		xmlFree(coords);

	return true;
}

/**
 * Free list of imagemap entries
 *
 * \param list Pointer to head of list
 */
void imagemap_freelist(struct mapentry *list)
{
	struct mapentry *entry, *prev;

	assert(list != NULL);

	entry = list;

	while (entry != 0) {
		prev = entry;
		free(entry->url);
		if (entry->target)
			free(entry->target);
		if (entry->type == IMAGEMAP_POLY) {
			free(entry->bounds.poly.xcoords);
			free(entry->bounds.poly.ycoords);
		}
		entry = entry->next;
		free(prev);
	}
}

/**
 * Retrieve url associated with imagemap entry
 *
 * \param c The containing content
 * \param key The map name to search for
 * \param x The left edge of the containing box
 * \param y The top edge of the containing box
 * \param click_x The horizontal location of the click
 * \param click_y The vertical location of the click
 * \param target Pointer to location to receive target pointer (if any)
 * \return The url associated with this area, or NULL if not found
 */
const char *imagemap_get(struct content *c, const char *key,
		unsigned long x, unsigned long y,
		unsigned long click_x, unsigned long click_y,
		const char **target)
{
	unsigned int slot = 0;
	struct imagemap *map;
	struct mapentry *entry;
	unsigned long cx, cy;

	assert(c != NULL);
	assert(c->type == CONTENT_HTML);
	if (key == NULL) return NULL;
	if (c->data.html.imagemaps == NULL) return NULL;

	slot = imagemap_hash(key);

	for (map = c->data.html.imagemaps[slot]; map != 0; map = map->next) {
		if (map->key != 0 && strcasecmp(map->key, key) == 0)
			break;
	}

	if (map == 0 || map->list == NULL) return NULL;

	for (entry = map->list; entry; entry = entry->next) {
		switch (entry->type) {
		case IMAGEMAP_DEFAULT:
			/* just return the URL. no checks required */
			if (target)
				*target = entry->target;
			return entry->url;
			break;
		case IMAGEMAP_RECT:
			if (click_x >= x + entry->bounds.rect.x0 &&
				    click_x <= x + entry->bounds.rect.x1 &&
				    click_y >= y + entry->bounds.rect.y0 &&
				    click_y <= y + entry->bounds.rect.y1) {
				if (target)
					*target = entry->target;
				return entry->url;
			}
			break;
		case IMAGEMAP_CIRCLE:
			cx = x + entry->bounds.circle.x - click_x;
			cy = y + entry->bounds.circle.y - click_y;
			if ((cx * cx + cy * cy) <=
				(unsigned long)(entry->bounds.circle.r *
					entry->bounds.circle.r)) {
				if (target)
					*target = entry->target;
				return entry->url;
			}
			break;
		case IMAGEMAP_POLY:
			if (imagemap_point_in_poly(entry->bounds.poly.num,
					entry->bounds.poly.xcoords,
					entry->bounds.poly.ycoords, x, y,
					click_x, click_y)) {
				if (target)
					*target = entry->target;
				return entry->url;
			}
			break;
		}
	}

	if (target)
		*target = NULL;

	return NULL;
}

/**
 * Hash function
 *
 * \param key The key to hash
 * \return The hashed value
 */
unsigned int imagemap_hash(const char *key)
{
	unsigned int z = 0;

	if (key == 0) return 0;

	for (; *key != 0; key++) {
		z += *key & 0x1f;
	}

	return (z % (HASH_SIZE - 1)) + 1;
}

/**
 * Test if a point lies within an arbitrary polygon
 * Modified from comp.graphics.algorithms FAQ 2.03
 *
 * \param num Number of vertices
 * \param xpt Array of x coordinates
 * \param ypt Array of y coordinates
 * \param x Left hand edge of containing box
 * \param y Top edge of containing box
 * \param click_x X coordinate of click
 * \param click_y Y coordinate of click
 * \return 1 if point is in polygon, 0 if outside. 0 or 1 if on boundary
 */
int imagemap_point_in_poly(int num, float *xpt, float *ypt, unsigned long x,
		unsigned long y, unsigned long click_x,
		unsigned long click_y)
{
	int i, j, c=0;

	assert(xpt != NULL);
	assert(ypt != NULL);

	for (i = 0, j = num-1; i < num; j = i++) {
		if ((((ypt[i]+y <= click_y) && (click_y < ypt[j]+y)) ||
		     ((ypt[j]+y <= click_y) && (click_y < ypt[i]+y))) &&
		     (click_x < (xpt[j] - xpt[i]) *
		     (click_y - (ypt[i]+y)) / (ypt[j] - ypt[i]) + xpt[i]+x))
			c = !c;
	}

	return c;
}
