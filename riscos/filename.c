/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *		  http://www.opensource.org/licenses/gpl-license
 * Copyright 2005 Richard Wilson <info@tinct.net>
 */

/** \file
 * Provides a central method of obtaining unique filenames.
 *
 * A maximum of 2^24 files can be allocated at any point in time.
 */

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "oslib/osfile.h"
#include "netsurf/riscos/filename.h"
#include "netsurf/utils/log.h"

#define FULL_WORD (unsigned int)4294967295
/* '0' + '0' * 10 */
#define START_PREFIX 528 

struct directory {
  	int numeric_prefix;		/** numeric representation of prefix */
	char prefix[10];		/** directory prefix, eg '00.11.52.' */
  	unsigned int low_used;		/** first 32 files, 1 bit per file */
  	unsigned int high_used;		/** last 32 files, 1 bit per file */
	struct directory *next;		/** next directory (sorted by prefix) */
};


static struct directory *root = NULL;
static char ro_filename_buffer[12];
static char ro_filename_directory[256];

static struct directory *ro_filename_create_directory(const char *prefix);


/**
 * Request a new, unique, filename.
 *
 * \return a pointer to a shared buffer containing the new filename
 */
char *ro_filename_request(void) {
	struct directory *dir;
	int i = -1;
	
	for (dir = root; dir; dir = dir->next)
		if ((dir->low_used & dir->high_used) != FULL_WORD) {
		  	if (dir->low_used != FULL_WORD) {
				for (i = 0; (dir->low_used & (1 << i)); i++);
		  	} else {
				for (i = 0; (dir->high_used & (1 << i)); i++);
		  		i += 32;
		  	}
			break;
		}
	if (i == -1) {
		/* no available slots - create a new directory */
		dir = ro_filename_create_directory(NULL);
		if (!dir) {
			LOG(("Failed to create a new directory."));
			return NULL;
		}
		i = 63;
	}
	if (i < 32)
		dir->low_used |= (1 << i);
	else
		dir->high_used |= (1 << (i - 32));
	sprintf(ro_filename_buffer, "%s%.2i", dir->prefix, i);
	return ro_filename_buffer;
}


/**
 * Claim a specific filename.
 *
 * \param  filename  the filename to claim
 * \return whether the claim was successful
 */
bool ro_filename_claim(const char *filename) {
	char dir_prefix[9];
	int file;
	struct directory *dir;
	
	/* filename format is always '01.23.45.XX' */
	strncpy(dir_prefix, filename, 9);
	dir_prefix[9] = '\0';
	file = (filename[10] + filename[9] * 10 - START_PREFIX);
	
	/* create the directory */
	dir = ro_filename_create_directory(dir_prefix);
	if (!dir)
		return false;

	/* update the entry */
  	if (file < 32) {
  	  	if (dir->low_used & (1 << file))
  	  		return false;
  		dir->low_used |= (1 << file);
  	} else {
  	  	if (dir->high_used & (1 << (file - 32)))
  	  		return false;
  		dir->high_used |= (1 << (file - 32));
  	}
  	return true;
}


/**
 * Releases a filename for future use.
 *
 * \param  filename  the filename to release
 */
void ro_filename_release(const char *filename) {
	struct directory *dir;
	int index, file;

	/* filename format is always '01.23.45.XX' */
	index = ((filename[7] + filename[6] * 10 - START_PREFIX) |
		((filename[4] + filename[3] * 10 - START_PREFIX) << 6) |
		((filename[1] + filename[0] * 10 - START_PREFIX) << 12));
	file = (filename[10] + filename[9] * 10 - START_PREFIX);
	
	/* modify the correct directory entry */
	for (dir = root; dir; dir = dir->next)
		if (dir->numeric_prefix == index) {
		  	if (file < 32)
		  		dir->low_used &= ~(1 << file);
		  	else
		  		dir->high_used &= ~(1 << (file - 32));
			return;
		}
}


/**
 * Initialise the filename provider.
 */
bool ro_filename_initialise(void) {
	/* create the 'CACHE_FILENAME_PREFIX' structure */
	xosfile_create_dir("<Wimp$ScrapDir>.WWW", 0);
	xosfile_create_dir("<Wimp$ScrapDir>.WWW.NetSurf", 0);
	xosfile_create_dir("<Wimp$ScrapDir>.WWW.NetSurf.Cache", 0);

	return true;
}


/**
 * Deletes all files in the cache directory that are not accounted for.
 */
void ro_filename_flush(void) {
	// todo: delete any files that aren't known of
	// todo: delete any empty references (?)
}


/**
 * Creates a new directory.
 *
 * \param  prefix  the prefix to use, or NULL to allocate a new one
 * \return a new directory structure, or NULL on memory exhaustion
 * 
 * Empty directories are never deleted, except by an explicit call to
 * ro_filename_flush().
 */
static struct directory *ro_filename_create_directory(const char *prefix) {
	char *last_1, *last_2;
	int index;
	struct directory *old_dir, *new_dir, *prev_dir = NULL;
	char dir_prefix[16];

	/* get the lowest unique prefix, or use the provided one */
	if (!prefix) {
		for (index = 0, old_dir = root; old_dir; index++,
				prev_dir = old_dir, old_dir = old_dir->next)
			if (old_dir->numeric_prefix != index)
				break;
		sprintf(dir_prefix, "%.2i.%.2i.%.2i.",
				((index >> 12) & 63),
				((index >> 6) & 63),
				((index >> 0) & 63));
		prefix = dir_prefix;
	} else {
		/* prefix format is always '01.23.45.' */
		index = ((prefix[7] + prefix[6] * 10 - START_PREFIX) |
			((prefix[4] + prefix[3] * 10 - START_PREFIX) << 6) |
			((prefix[1] + prefix[0] * 10 - START_PREFIX) << 12));
		for (old_dir = root; old_dir; prev_dir = old_dir,
				old_dir = old_dir->next) {
			if (old_dir->numeric_prefix == index)
				return old_dir;
			else if (old_dir->numeric_prefix > index)
				break;
		}
	}

	/* allocate a new directory */
	new_dir = (struct directory *)malloc(sizeof(struct directory));
	if (!new_dir) {
		LOG(("No memory for malloc()"));
		return NULL;
	}
	strncpy(new_dir->prefix, prefix, 9);
	new_dir->prefix[9] = '\0';
	new_dir->low_used = new_dir->high_used = 0;
	new_dir->numeric_prefix = index;

	if (!prev_dir) {
		new_dir->next = root;
		root = new_dir;
	} else {
		new_dir->next = prev_dir->next;
		prev_dir->next = new_dir;
	}
	
	/* create the directory structure */
	sprintf(ro_filename_directory, "%s.", CACHE_FILENAME_PREFIX);
	last_1 = ro_filename_directory + strlen(CACHE_FILENAME_PREFIX) + 1;
	last_2 = new_dir->prefix;
	for (int i = 0; i < 3 && *last_2; i++) {
		*last_1++ = *last_2++;
		while (*last_2 && *last_2 != '.')
			*last_1++ = *last_2++;
		if (*last_2) {
			last_1[0] = '\0';
			xosfile_create_dir(ro_filename_directory, 0);
		}
	}
	return new_dir;
}
