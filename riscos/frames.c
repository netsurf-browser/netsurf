/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 John M Bell <jmb202@ecs.soton.ac.uk>
 */

#include <stdbool.h>

#include "netsurf/riscos/frames.h"
#ifndef TEST
#  define NDEBUG
#endif
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

#ifdef TEST
#  include <stdio.h>
#endif

static struct frame_list fl = {0, 0, &fl, &fl};
static struct frame_list *flist = &fl;

/* -------------------------------------------------------------------- *
 * List handling stuff                                                  *
 * -------------------------------------------------------------------- */

/**
 * Adds a new frameset associated with a browser window to the list
 */
void frameset_add_to_list(struct browser_window *bw, struct frame *frame) {

         struct frame_list *nfl = xcalloc(1, sizeof(*nfl));

         LOG(("adding %p to list", frame));
         nfl->frame = frame;
         nfl->bw = bw;
         nfl->prev = flist->prev;
         nfl->next = flist;
         flist->prev->next = nfl;
         flist->prev = nfl;
}

/**
 * Removes a frameset associated with a browser window to the list
 */
void frameset_remove_from_list(struct browser_window *bw) {

         struct frame_list *temp = frameset_get_from_list(bw);
         if(temp != NULL) {
                 delete_tree(temp->frame);
                 temp->prev->next = temp->next;
                 temp->next->prev = temp->prev;
                 xfree(temp);
         }
}

/**
 * Retrieves a frameset from the list
 * Returns a frame_list struct pointer or NULL is nothing is found
 */
struct frame_list *frameset_get_from_list(struct browser_window *bw) {

         struct frame_list *nfl;

         for(nfl = flist->next; (nfl != flist) && (nfl->bw != bw);
               nfl = nfl->next)
               ;

         if(nfl != flist)
               return nfl;

         return NULL;
}

/**
 * Retrieves a frame from the list/tree structure.
 * Returns a frame struct pointer or NULL if nothing is found
 */
struct frame *frame_get_from_list(struct browser_window *bw, struct box *b,
                                  bool strict) {

         struct frame_list *nfl;
         struct frame *f=0;

         for(nfl = flist->next; (nfl != flist); nfl = nfl->next) {
               LOG(("checking tree %p",nfl->frame));
               if ((f = get_frame_from_tree(nfl->frame, bw, b, strict))) {
                      LOG(("returning f: %p", f));
                      return f;
               }
         }

         return NULL;
}

/* -------------------------------------------------------------------- *
 * Tree handling stuff                                                  *
 * -------------------------------------------------------------------- */

/**
 * Adds a new frame to the tree
 * Creates a new tree if appropriate.
 */
void add_frame_to_tree (struct browser_window *parent_bw, struct box *box,
                        struct browser_window *bw, struct content *c,
                        char *name) {

        struct frame *nrf;
        struct frame *parent;
        struct frame *nf = xcalloc(1, sizeof(*nf));
        unsigned int i;

        /* get parent node */
        if ((parent = frame_get_from_list(parent_bw, box, false)) == NULL) {

          LOG(("no tree found - creating new"));
          nrf = xcalloc(1, sizeof(*nrf));
          nrf->win = parent_bw;
          nrf->box = (struct box*)-1;
          nrf->c = 0;
          nrf->name = xstrdup("_top");
          nrf->parent = 0;
          nrf->no_children = 0;
          nrf->children = xcalloc(0,1);
          frameset_add_to_list(parent_bw, nrf);
          parent = frame_get_from_list(parent_bw, (struct box*)-1, true);
        }

        LOG(("got parent"));
        nf->win = bw;
        nf->box = box;
        nf->c = c;
        nf->name = xstrdup(name);
        nf->parent = parent;
        nf->no_children = 0;

        LOG(("adding frame to tree"));
        i = parent->no_children;
        parent->children =
               xrealloc(parent->children, (i+1) * sizeof(struct frame));
        parent->children[i] = nf;
        parent->no_children++;
}

/**
 * Retrieves a frame from the tree.
 * If 'strict' is true, tests for both bw and box. Otherwise, just tests bw.
 * Returns the frame or NULL.
 */
struct frame *get_frame_from_tree(struct frame *root,
                                  struct browser_window *bw,
                                  struct box *b,
                                  bool strict) {

        int i;

        if (!root) {
          LOG(("tree was empty"));
          return NULL; /* empty tree */
        }

        if (strict) {
          if (root->parent != 0) { /* has a parent node => check that */
            LOG(("(%p, %p),(%p, %p)", root->parent->win, bw, root->box, b));
            if (root->parent->win == bw && root->box == b) { /* found frame */
                LOG(("found frame %p", root));
                return root;
            }
          }
          else { /* no parent node => check this one */
            LOG(("(%p, %p),(%p, %p)", root->win, bw, root->box, b));
            if (root->win == bw && root->box == b) { /* found frame */
                LOG(("found frame %p", root));
                return root;
            }
          }
        }
        else {
          if (root->parent != 0) { /* has a parent node => check that */
            LOG(("(%p, %p),(%p, %p)", root->parent->win, bw, root->box, b));
            if (root->parent->win == bw) { /* found frame */
                LOG(("found frame %p", root));
                return root;
            }
            else if (root->win == bw) {
                LOG(("adding as child of '%s'", root->name));
                return root;
            }
          }
          else { /* no parent node => check this one */
            LOG(("(%p, %p),(%p, %p)", root->win, bw, root->box, b));
            if (root->win == bw) { /* found frame */
                LOG(("found frame %p", root));
                return root;
            }
          }
        }

        if (root->no_children == 0)
            return NULL;

        for (i=0; i!=root->no_children; i++) { /* not found so recurse */
            return get_frame_from_tree(root->children[i], bw, b, strict);
        }

        return NULL;
}

/**
 * Deletes a complete tree.
 */
void delete_tree(struct frame *root) {

        int i;

        if (!root) return; /* empty tree */

        if (root->no_children == 0) { /* leaf node => delete and return */
            LOG(("deleting '%s'", root->name));
            xfree(root->name);
            xfree(root);
            return;
        }

        for (i=0; i!=root->no_children; i++) { /* non-leaf node
                                                  => kill children */
            delete_tree(root->children[i]);
        }

        /* all children killed => remove this node */
        LOG(("deleting '%s'", root->name));
        xfree(root->name);
        xfree(root);
}



#ifdef TEST
void traverse_tree(struct frame *root, int depth) {

        int i;

        if (!root) return; /* empty tree */

        if (root->no_children == 0) {
          for (i=0; i!=(depth+1); i++)
            printf("\t");
          printf("frame: %p (%s)\n", root, root->name);
          return;
        }
        else {
          for(i=0; i!=(depth+1); i++)
            printf("\t");
          printf("frame: %p (%s)\n", root, root->name);
        }

        for (i=0; i!=root->no_children; i++) { /* not found so recurse */
            traverse_tree(root->children[i], (depth+1));
        }

        return;
}

void dump_all_frame_moose(void) {

        struct frame_list *nfl;

        for (nfl=flist->next; nfl!=flist; nfl=nfl->next) {
          printf("bw: %p\n", nfl->bw);
          traverse_tree(nfl->frame, 0);
          printf("\n");
        }
}

int main (void) {

  struct frame *f;

  add_frame_to_tree(1, 1, 1, 3, "test");
  add_frame_to_tree(0, 2, 2, 5, "moo");
  add_frame_to_tree(0, 3, 3, 1, "moose");
  add_frame_to_tree(2, 4, 4, 2, "elk");

  dump_all_frame_moose();

  f = frame_get_from_list(1, 1, true);
  if (f)
    printf("%s: %d, %d\n", f->name, f->win, f->c);
  else
    printf("couldn't find 1,1\n");

  frameset_remove_from_list(1);

  f = frame_get_from_list(2, 4, true);
  if (f)
    printf("%s: %d, %d\n", f->name, f->win, f->c);
  else
    printf("couldn't find 2,4\n");

  dump_all_frame_moose();

  return 0;
}
#endif
