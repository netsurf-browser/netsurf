/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 John M Bell <jmb202@ecs.soton.ac.uk>
 *
 * Much of this shamelessly copied from utils/messages.c
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include "netsurf/content/content.h"
#include "netsurf/desktop/imagemap.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#define HASH_SIZE 31 /* fixed size hash table */

typedef enum {IMAGEMAP_DEFAULT, IMAGEMAP_RECT, IMAGEMAP_CIRCLE, IMAGEMAP_POLY } imagemap_entry_type;

struct mapentry {
        imagemap_entry_type type;             /**< type of shape */
        char *url;                            /**< url to go to */
        union {
                struct {
                        int x;                /**< x coordinate of centre */
                        int y;                /**< y coordinate of center */
                        int r;                /**< radius of circle */
                } circle;
                struct {
                        int x0;               /**< left hand edge */
                        int y0;               /**< top edge */
                        int x1;               /**< right hand edge */
                        int y1;               /**< bottom edge */
                } rect;
                struct {
                        int num;              /**< number of points */
                        float *xcoords;       /**< x coordinates */
                        float *ycoords;       /**< y coordinates */
                } poly;
        } bounds;
        struct mapentry *next;                /**< next entry in list */
};

struct imagemap {
        char *key;              /**< key for this entry */
        struct mapentry *list;  /**< pointer to linked list of entries */
        struct imagemap *next;  /**< next entry in this hash chain */
};

static void imagemap_add(struct content *c, const char *key,
                         struct mapentry *list);
static void imagemap_create(struct content *c);
static struct mapentry *imagemap_extract_map(xmlNode *node, struct content *c,
                                 struct mapentry *entry);
static struct mapentry *imagemap_addtolist(xmlNode *n, struct mapentry *entry);
static void imagemap_freelist(struct mapentry *list);
static unsigned int imagemap_hash(const char *key);
static int imagemap_point_in_poly(int num, float *xpt, float *ypt, unsigned long x, unsigned long y, unsigned long click_x, unsigned long click_y);

/**
 * Add an imagemap to the hashtable, creating it if it doesn't exist
 *
 * @param c The containing content
 * @param key The name of the imagemap
 * @param list List of map regions
 */
void imagemap_add(struct content *c, const char *key, struct mapentry *list) {

        struct imagemap *map;
        unsigned int slot;

        assert(c->type == CONTENT_HTML);

        imagemap_create(c);

        map = xcalloc(1, sizeof(*map));
        map->key = xstrdup(key);
        map->list = list;
        slot = imagemap_hash(key);
        map->next = c->data.html.imagemaps[slot];
        c->data.html.imagemaps[slot] = map;
}

/**
 * Create hashtable of imagemaps
 *
 * @param c The containing content
 */
void imagemap_create(struct content *c) {

        assert(c->type == CONTENT_HTML);

        if (c->data.html.imagemaps == 0) {
                c->data.html.imagemaps = xcalloc(HASH_SIZE,
                                                 sizeof(struct imagemap));
        }
}

/**
 * Destroy hashtable of imagemaps
 *
 * @param c The containing content
 */
void imagemap_destroy(struct content *c) {

        unsigned int i;

        assert(c->type == CONTENT_HTML);

        /* no imagemaps -> return */
        if (c->data.html.imagemaps == 0) return;

        for (i = 0; i != HASH_SIZE; i++) {
                struct imagemap *map, *next;
                map = c->data.html.imagemaps[i];
                while (map != 0) {
                       next = map->next;
                       imagemap_freelist(map->list);
                       xfree(map->key);
                       xfree(map);
                       map = next;
                }
        }

        xfree(c->data.html.imagemaps);
}

/**
 * Dump imagemap data to the log
 *
 * @param c The containing content
 */
void imagemap_dump(struct content *c) {

        unsigned int i;

        int j;

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
                                         LOG(("\tPolygon: %s:",
                                               entry->url));
                                         for (j=0;
                                              j!=entry->bounds.poly.num;
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
 * @param node Root node of tree
 * @param c The containing content
 */
void imagemap_extract(xmlNode *node, struct content *c) {

        xmlNode *this;
        struct mapentry *entry = 0;
        char *name;

        if (node->type == XML_ELEMENT_NODE) {
                if (strcmp(node->name, "map") == 0) {
                        if (!(name = (char*)xmlGetProp(node,
                                                    (const xmlChar*)"name")))
                                return;
                        entry = imagemap_extract_map(node, c, entry);
                        imagemap_add(c, name, entry);
                        xmlFree(name);
                        return;
                }
        }
        else return;

        /* now recurse */
        for (this = node->children; this != 0; this = this->next) {
                imagemap_extract(this, c);
        }
}

struct mapentry *imagemap_extract_map(xmlNode *node, struct content *c, struct mapentry *entry) {

        xmlNode *this;

        if (node->type == XML_ELEMENT_NODE) {
                /** \todo ignore <area> elements if there are other
                 *        block-level elements present in map
                 */
                if (strcmp(node->name, "area") == 0 ||
                    strcmp(node->name, "a") == 0) {
                        entry = imagemap_addtolist(node, entry);
                        return entry;
                }
        }
        else return entry;

        for (this = node->children; this != 0; this = this->next) {
                entry = imagemap_extract_map(this, c, entry);
        }

        return entry;
}

struct mapentry *imagemap_addtolist(xmlNode *n, struct mapentry *entry) {

        char *shape, *coords = 0, *href, *val;
        int num;
        struct mapentry *new, *temp;

        if (strcmp(n->name, "area") == 0) {
                /* nohref attribute present - ignore this entry */
                if (xmlGetProp(n, (const xmlChar*)"nohref") != 0) {
                        return entry;
                }
        }
        /* no href -> ignore */
        if (!(href = (char*)xmlGetProp(n, (const xmlChar*)"href"))) {
                return entry;
        }
        /* no shape -> shape is a rectangle */
        if (!(shape = (char*)xmlGetProp(n, (const xmlChar*)"shape"))) {
                xmlFree(shape);
                shape = (char*)xmlMemStrdup("rect");
        }
        if (strcasecmp(shape, "default") != 0) {
                /* no coords -> ignore */
                if (!(coords = (char*)xmlGetProp(n, (const xmlChar*)"coords"))) {
                        xmlFree(href);
                        xmlFree(shape);
                        return entry;
                }
        }

        new = xcalloc(1, sizeof(*new));

        /* extract area shape */
        if (strcasecmp(shape, "rect") == 0) {
                new->type = IMAGEMAP_RECT;
        }
        else if (strcasecmp(shape, "circle") == 0) {
                new->type = IMAGEMAP_CIRCLE;
        }
        else if (strcasecmp(shape, "poly") == 0) {
                new->type = IMAGEMAP_POLY;
        }
        else if (strcasecmp(shape, "default") == 0) {
                new->type = IMAGEMAP_DEFAULT;
        }
        else { /* unknown shape -> bail */
                xfree(new);
                xmlFree(href);
                xmlFree(shape);
                xmlFree(coords);
                return entry;
        }

        new->url = xstrdup(href);

        if (new->type != IMAGEMAP_DEFAULT) {

                /* coordinates are a comma-separated list of values */
                val = strtok(coords, ",");
                num = 1;

                switch (new->type) {
                        case IMAGEMAP_RECT:
                                /* (left, top, right, bottom) */
                                while (val && num <= 4) {
                                        switch (num) {
                                                case 1:
                                                    new->bounds.rect.x0 = atoi(val);
                                                    break;
                                                case 2:
                                                    new->bounds.rect.y0 = atoi(val);
                                                    break;
                                                case 3:
                                                    new->bounds.rect.x1 = atoi(val);
                                                    break;
                                                case 4:
                                                    new->bounds.rect.y1 = atoi(val);
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
                                                    new->bounds.circle.x = atoi(val);
                                                    break;
                                                case 2:                                                                          new->bounds.circle.y = atoi(val);
                                                    break;
                                                case 3:
                                                    new->bounds.circle.r = atoi(val);
                                                    break;
                                        }
                                        num++;
                                        val = strtok('\0', ",");
                                }
                                break;
                        case IMAGEMAP_POLY:
                                new->bounds.poly.xcoords =
                                                 xcalloc(0, sizeof(*new->bounds.poly.xcoords));
                                new->bounds.poly.ycoords =
                                                 xcalloc(0, sizeof(*new->bounds.poly.ycoords));
                                int x, y;
                                while (val) {
                                        x = atoi(val);
                                        val = strtok('\0', ",");
                                        if (!val) break;
                                        y = atoi(val);

                                        new->bounds.poly.xcoords =
                                                xrealloc(new->bounds.poly.xcoords,
                                                         num*sizeof(*new->bounds.poly.xcoords));
                                        new->bounds.poly.ycoords =
                                                xrealloc(new->bounds.poly.ycoords,
                                                         num*sizeof(*new->bounds.poly.ycoords));
                                        new->bounds.poly.xcoords[num-1] = x;
                                        new->bounds.poly.ycoords[num-1] = y;

                                        num++;
                                        val = strtok('\0', ",");
                                }

                                new->bounds.poly.num = num-1;

                                break;
                        default:
                                break;
                }
        }

        new->next = 0;
        if (entry) {
                /* add to END of list */
                for (temp = entry; temp->next != 0; temp = temp->next)
                        ;
                temp->next = new;
        }
        else {
                entry = new;
        }

        xmlFree(href);
        xmlFree(shape);
        xmlFree(coords);

        return entry;
}

/**
 * Free list of imagemap entries
 *
 * @param list Pointer to head of list
 */
void imagemap_freelist(struct mapentry *list) {

        struct mapentry *entry, *prev;

        entry = list;

        while (entry != 0) {
                prev = entry;
                xfree(entry->url);
                if (entry->type == IMAGEMAP_POLY) {
                        xfree(entry->bounds.poly.xcoords);
                        xfree(entry->bounds.poly.ycoords);
                }
                entry = entry->next;
                xfree(prev);
        }
}

/**
 * Retrieve url associated with imagemap entry
 *
 * @param c The containing content
 * @param key The map name to search for
 * @param x The left edge of the containing box
 * @param y The top edge of the containing box
 * @param click_x The horizontal location of the click
 * @param click_y The vertical location of the click
 * @return The url associated with this area, or NULL if not found
 */
char *imagemap_get(struct content *c, const char *key, unsigned long x,
         unsigned long y, unsigned long click_x, unsigned long click_y) {

        unsigned int slot = 0;
        struct imagemap *map;
        struct mapentry *entry;
        unsigned long cx, cy;

        assert(c->type == CONTENT_HTML);

        slot = imagemap_hash(key);

        for (map = c->data.html.imagemaps[slot];
             map != 0 && strcasecmp(map->key, key) != 0;
             map = map->next)
                ;

        if (map == 0) return NULL;

        for (entry = map->list; entry; entry = entry->next) {
                switch (entry->type) {
                        case IMAGEMAP_DEFAULT:
                                /* just return the URL. no checks required */
                                return entry->url;
                                break;
                        case IMAGEMAP_RECT:
                                if (click_x >= x + entry->bounds.rect.x0 &&
                                    click_x <= x + entry->bounds.rect.x1 &&
                                    click_y >= y + entry->bounds.rect.y0 &&
                                    click_y <= y + entry->bounds.rect.y1) {
                                        return entry->url;
                                }
                                break;
                        case IMAGEMAP_CIRCLE:
                                cx = x + entry->bounds.circle.x - click_x;
                                cy = y + entry->bounds.circle.y - click_y;
                                if ((cx*cx + cy*cy) <=
                                    (unsigned long)(entry->bounds.circle.r*
                                     entry->bounds.circle.r)) {
                                        return entry->url;
                                }
                                break;
                        case IMAGEMAP_POLY:
                                if (imagemap_point_in_poly(entry->bounds.poly.num, entry->bounds.poly.xcoords, entry->bounds.poly.ycoords, x, y, click_x, click_y)) {
                                        return entry->url;
                                }
                                break;
                }
        }

        return NULL;
}

/**
 * Hash function
 *
 * @param key The key to hash
 * @return The hashed value
 */
unsigned int imagemap_hash(const char *key) {

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
 * @param num Number of vertices
 * @param xpt Array of x coordinates
 * @param ypt Array of y coordinates
 * @param x Left hand edge of containing box
 * @param y Top edge of containing box
 * @param click_x X coordinate of click
 * @param click_y Y coordinate of click
 * @return 1 if point is in polygon, 0 if outside. 0 or 1 if on boundary
 */
int imagemap_point_in_poly(int num, float *xpt, float *ypt, unsigned long x, unsigned long y, unsigned long click_x, unsigned long click_y) {

        int i, j, c=0;

        for (i = 0, j = num-1; i < num; j = i++) {
                if ((((ypt[i]+y <= click_y) && (click_y < ypt[j]+y)) ||
                     ((ypt[j]+y <= click_y) && (click_y < ypt[i]+y))) &&
                     (click_x < (xpt[j] - xpt[i]) *
                     (click_y - (ypt[i]+y)) / (ypt[j] - ypt[i]) + xpt[i]+x))
                        c = !c;
        }

        return c;
}
