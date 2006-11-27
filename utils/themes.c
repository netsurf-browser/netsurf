/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 */

#include "netsurf/utils/themes.h"

struct theme_descriptor;

/**
 * Initialise the themes interface.  This must be called once only before any
 * other themes_ functions are called.  It will enumerate all the themes
 * found in the directory path names passed.  Further themes can be added at
 * runtime by using themes_add_new().
 *
 * \param directories An array of directory path names that will be scanned
 *                    in turn to find installed themes.  Earlier directories
 *                    take precedence over ones mentioned later.
 */
void themes_initialise(const unsigned char *directories[])
{

}

/**
 * Close the themes system, freeing any open themes, and destroying any
 * associated bitmaps that have been created from it.
 */
void themes_finalise(void)
{

}

/**
 * Add a new theme to the themes system.  This is primarily of use to let
 * NetSurf know about themes that were installed after NetSurf started - for
 * example, from the "Install Theme" user interface.
 *
 * \param filename The filename, including full directory path, to the new
 *                 theme.  This theme will superceed any previous theme
 *                 that has the same theme name.
 */
void themes_add_new(const unsigned char *filename);

/**
 * Open a theme for use.  This increases the theme's use-count.
 *
 * \param themename Name of the theme to open.  This is just the theme
 *                  name, not the file name.
 * \return struct theme_descriptor for use with other calls when refering to
 *         this theme.  NULL if the theme is unknown to the theme system.
 */
struct theme_descriptor *themes_open(const unsigned char *themename)
{

}

/**
 * Decreases a theme's usage count, freeing memory assoicated with it should
 * it reach zero.  This will also destroy any bitmaps that have been created
 * from it.
 *
 * \param theme Theme that the caller is finished with.
 */
void themes_close(struct theme_descriptor *theme)
{

}

/**
 * Enumerate known themes
 * \param ctx Set to NULL for initial call.  It will be updated after each call
 *            to point to the next result.
 * \return struct theme_descriptor for use with other calls, or NULL if there
 *         are no more themes known.  Note that this does not increase the
 *         theme's usage count - you should call themes_open() with the result
 *         of themes_get_name() if you wish to use this theme.
 */
struct theme_descriptor *themes_enumerate(void **ctx)
{

}

/**
 * Return a NetSurf bitmap structure for an image stored within the theme
 * container.  Note that this bitmap will become invalid if the theme's usage
 * count reaches zero due to calls to themes_close()
 *
 * \param theme The theme you wish to retrieve an image from.  If this is NULL,
 *        the default theme, as set by themes_set_default(), will be used.
 * \param name  The name of the image within the theme you wish to retrieve.
 * \return struct bitmap containing the decoded image.
 */
struct bitmap *theme_get_image(struct theme_descriptor *theme,
				const unsigned char *name)
{

}

/**
 * Set the default theme to use.  This calls themes_open() to increase the
 * theme's usage count.  It will also call themes_close() on the previous
 * default, if there was one.
 *
 * \param themename The name of the theme to set as the new default.
 */
void themes_set_default(const unsigned char *themename)
{

}

/**
 * Return the name of a theme from a struct theme_descriptor.
 *
 * \param theme The theme whose name to return
 * \return Name of theme
 */
const unsigned char *themes_get_name(struct theme_descriptor *theme)
{

}

/**
 * Return the author of a theme from a struct theme_descriptor.
 *
 * \param theme The theme whose author to return
 * \return Author of the theme.
 */
const unsigned char *themes_get_author(struct theme_descriptor *theme)
{

}
