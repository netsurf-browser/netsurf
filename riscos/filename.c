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
#include "oslib/osgbpb.h"
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
static bool ro_filename_flush_directory(const char *folder, int depth);
static bool ro_filename_delete_recursive(char *folder);

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
	xhourglass_on();
	while (ro_filename_flush_directory(CACHE_FILENAME_PREFIX, 0));
	xhourglass_off();
}


/**
 * Deletes some files in a directory that are not accounted for.
 *
 * A single call to this function may not delete all the files in
 * a directory. It should be called until it returns false.
 *
 * \param folder	the folder to search
 * \param depth		the folder depth
 * \returns whether further calls may be needed
 */
bool ro_filename_flush_directory(const char *folder, int depth) {
	bool changed = false;
	bool del;
	int number, i;
	int prefix = 0;
	unsigned int prefix_mask = (63 << 12);
	int context = 0;
	int read_count;
	osgbpb_INFO(100) info;
	os_error *error;
	char child[256];
	const char *prefix_start = NULL;
	struct directory *dir = NULL;

	/* find out directory details */
	if (depth > 0)
		prefix_start = folder + strlen(folder) - depth * 3 + 1;
	for (i = 0; ((i < depth) && (i < 3)); i++) {
	  	number = prefix_start[1] + prefix_start[0] * 10 - START_PREFIX;
		prefix |= (number << (12 - i * 6));
		prefix_mask |= (63 << (6 - i * 6));
		prefix_start += 3;
	}
	if (depth == 3) {
		for (dir = root; dir; dir = dir->next)
			if (dir->numeric_prefix == prefix)
				break;
		if ((!dir) || (dir->numeric_prefix != prefix))
			return false;
	}

	while (context != -1) {
		/* read some directory info */
		error = xosgbpb_dir_entries_info(folder,
				(osgbpb_info_list *) &info, 1, context,
				sizeof(info), 0, &read_count, &context);
		if (error) {
			LOG(("xosgbpb_dir_entries_info: 0x%x: %s",
				error->errnum, error->errmess));
			if (error->errnum == 0xd6)	/* no such dir */
				return false;
			break;
		}
		/* ensure we read some data */
		if (read_count == 0)
			continue;
		/* first 3 depths are directories only, then files only */
		del = false;
		if (depth < 3) {
			if (info.obj_type != fileswitch_IS_DIR)
				del = true;
		} else {
			if (info.obj_type != fileswitch_IS_FILE)
				del = true;
		}
		/* check we are a file numbered '00' -> '63' */
        	if ((!del) && (info.name[0] >= '0') && (info.name[0] <= '6') &&
				(info.name[1] >= '0') && (info.name[1] <= '9') &&
				(info.name[2] == '\0')) {
			number = atoi(info.name);
			if ((number >= 0) && (number <= 63)) {
				if (depth == 3) {
					if (number < 32)
						del = !(dir->low_used &
							(1 << number));
					else
						del = !(dir->high_used &
							(1 << (number - 32)));
				} else {
				  	del = true;
					prefix &= ~(63 << (12 - depth * 6));
					prefix |= (number << (12 - depth * 6));
					for (dir = root; dir; dir = dir->next) {
						number = dir->numeric_prefix &
								prefix_mask;
						if (number == prefix) {
							del = false;
							break;
						}
					}
				}
			} else {
				del = true;
			}
		} else {
			del = true;
		}
		/* continue if we are a valid reference so far */
       		if ((!del) && (info.obj_type != fileswitch_IS_DIR))
			continue;
		/* delete or recurse */
		snprintf(child, 256, "%s.%s", folder, info.name);
		child[255] = '\0';
		if (del) {
			if (info.obj_type == fileswitch_IS_DIR)
				ro_filename_delete_recursive(child);
			error = xosfile_delete(child, 0, 0, 0, 0, 0);
			if (error)
				LOG(("xosfile_delete: 0x%x: %s",
					error->errnum, error->errmess));
			else
				changed = true;
		} else {
			while (ro_filename_flush_directory(child, depth + 1));
		}
	}
	return changed;
}


/**
 * Recursively deletes the contents of a directory
 *
 * \param directory	the directory to delete
 * \return true on success, false otherwise
 */
bool ro_filename_delete_recursive(char *folder) {
	int context = 0;
	int read_count;
	osgbpb_INFO(100) info;
	os_error *error;
	char child[256];

	while (context != -1) {
		/* read the first entry */
		error = xosgbpb_dir_entries_info(folder,
				(osgbpb_info_list *) &info, 1, 0,
				sizeof(info), 0, &read_count, &context);
		if (error) {
			LOG(("xosgbpb_dir_entries_info: 0x%x: %s",
				error->errnum, error->errmess));
			if (error->errnum == 0xd6)	/* no such dir */
				return false;
			break;
		}
		/* ensure we read some data */
		if (read_count == 0)
			continue;
		snprintf(child, 256, "%s.%s", folder, info.name);
		/* recurse for files */
		if (info.obj_type == fileswitch_IS_DIR) {
		  	if (!ro_filename_delete_recursive(child))
		  		return false;
		}
		error = xosfile_delete(child, 0, 0, 0, 0, 0);
		if (error) {
			LOG(("xosfile_delete: 0x%x: %s",
				error->errnum, error->errmess));
			return false;
		}
  	}
  	return true;
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
	os_error *error;

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

	/* if the previous directory has the same parent then we can simply
	 * create the child. */
	if ((prev_dir) && (!strncmp(prev_dir->prefix, new_dir->prefix, 6))) {
		new_dir->prefix[8] = '\0';
		sprintf(ro_filename_directory, "%s.%s",
				CACHE_FILENAME_PREFIX, new_dir->prefix);
		new_dir->prefix[8] = '.';
		error = xosfile_create_dir(ro_filename_directory, 0);
		/* the user has probably deleted the parent directory whilst
		 * we are running if there is an error, so we don't report this
		 * yet and try to create the structure normally. */
		if (!error)
			return new_dir;
		LOG(("xosfile_create_dir: 0x%x: %s",
				error->errnum, error->errmess));
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
			error = xosfile_create_dir(ro_filename_directory, 0);
			if (error) {
				LOG(("xosfile_create_dir: 0x%x: %s",
						error->errnum, error->errmess));
				return NULL;
			}
		}
	}
	return new_dir;
}
