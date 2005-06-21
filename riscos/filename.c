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

struct directory {
	char prefix[10];		/** directory prefix, eg '00.11.52.' */
  	unsigned int low_used;		/** first 32 files, 1 bit per file */
  	unsigned int high_used;		/** last 32 files, 1 bit per file */
  	unsigned int low_persistent;	/** first 32 files, 1 bit per file */
  	unsigned int high_persistent;	/** last 32 files, 1 bit per file */
	struct directory *next;		/** next directory (sorted by prefix) */
};


static struct directory *root = NULL;
static char ro_filename_buffer[12];
static char ro_filename_directory[256];

static struct directory *ro_filename_create_directory(const char *prefix);


/**
 * Request a new, unique, filename.
 *
 * \param  persistent  keep the filename allocated across sessions
 * \return a pointer to a shared buffer containing the new filename
 */
char *ro_filename_request(bool persistent) {
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
	if (i < 32) {
		dir->low_used |= (1 << i);
		if (persistent) 
			dir->low_persistent |= (1 << i);
	} else {
		dir->high_used |= (1 << (i - 32));
		if (persistent) 
			dir->high_persistent |= (1 << (i - 32));
	}
	sprintf(ro_filename_buffer, "%s%i", dir->prefix, i);
	return ro_filename_buffer;
}


/**
 * Releases a filename for future use.
 *
 * \param  filename  the filename to release
 */
void ro_filename_release(const char *filename) {
  	char *last;
	char dir_prefix[16];
	int i;
	struct directory *dir;
	
	/* extract the prefix */
	sprintf(dir_prefix, filename);
	for (i = 0, last = dir_prefix; i < 3; i++)
		while (*last++ != '.');
	i = atoi(last);
	last[0] = '\0';
	
	/* modify the correct directory entry */
	for (dir = root; dir; dir = dir->next)
		if (!strcmp(dir->prefix, dir_prefix)) {
		  	if (i < 32) {
		  		dir->low_used &= ~(1 << i);
		  		dir->low_persistent &= ~(1 << i);
		  	} else {
		  		dir->high_used &= ~(1 << (i - 32));
		  		dir->high_persistent &= ~(1 << (i - 32));
		  	}
			return;
		}
}


/**
 * Initialise the filename provider and load the previous session state.
 */
bool ro_filename_initialise(void) {
	char s[16];
  	struct directory *dir;
	FILE *fp;
	int version;

	/* create the 'CACHE_FILENAME_PREFIX' structure */
	xosfile_create_dir("<Wimp$ScrapDir>.WWW", 0);
	xosfile_create_dir("<Wimp$ScrapDir>.WWW.NetSurf", 0);
	xosfile_create_dir("<Wimp$ScrapDir>.WWW.NetSurf.Cache", 0);

	/* load the persistent file list */
	fp = fopen("Choices:WWW.NetSurf.Filename", "r");
	if (!fp) {
		LOG(("Unable to open filename record for reading."));
		return true;
	}

	if (!fgets(s, 16, fp)) {
	  	fclose(fp);
		return false;
	}
	version = atoi(s);
	if (version != 100) {
	  	LOG(("Invalid or unsupported filename record."));
	  	fclose(fp);
		return false;
	}
	while (fgets(s, 16, fp)) {
		if (s[strlen(s) - 1] == '\n')
			s[strlen(s) - 1] = '\0';
		dir = ro_filename_create_directory(s);
		if (!dir) {
			LOG(("Unable to load filename record for prefix '%s'.",
					s));
		  	fclose(fp);
			return false;
		}
		if (!fgets(s, 16, fp)) {
		  	fclose(fp);
			return false;
		}
		dir->low_used = dir->low_persistent = (unsigned int)
				strtoul(s, (char **)NULL, 10);
		if (!fgets(s, 16, fp)) {
		  	fclose(fp);
			return false;
		}
		dir->high_used = dir->high_persistent = (unsigned int)
				strtoul(s, (char **)NULL, 10);
	}
  	fclose(fp);

	ro_filename_finalise();
	return true;
}


/**
 * Finalise the filename provider and save the previous session state.
 */
bool ro_filename_finalise(void) {
	struct directory *dir;
	FILE *fp;

	fp = fopen("<Choices$Write>.WWW.NetSurf.Filename", "w");
	if (!fp) {
		LOG(("Unable to open filename record for writing."));
		return false;
	}
	fprintf(fp, "100\n");

	/* dump the persistent files in the format of:
	 * [prefix]\n[low used word]\n[high used word]\n */
	for (dir = root; dir; dir = dir->next)
		if ((dir->low_persistent | dir->high_persistent) != 0)
			fprintf(fp, "%s\n%u\n%u\n",
					dir->prefix,
					dir->low_persistent,
					dir->high_persistent);
	fclose(fp);
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
	int result, index;
	struct directory *old_dir, *new_dir, *prev_dir = NULL;
	char dir_prefix[16];
	
	/* get the lowest unique prefix, or use the provided one */
	if (prefix) {
		strcpy(dir_prefix, prefix);
	} else {
		sprintf(dir_prefix, "00.00.00.");
		for (index = 1, old_dir = root; old_dir;
				index++, old_dir = old_dir->next) {
			if (strcmp(old_dir->prefix, dir_prefix))
				break;
			sprintf(dir_prefix, "%.2i.%.2i.%.2i.",
					((index >> 12) & 63),
					((index >> 6) & 63),
					((index >> 0) & 63));
		}
	}
	
	/* allocate a new directory */
	new_dir = (struct directory *)calloc(1,
			sizeof(struct directory));
	if (!new_dir) {
		LOG(("No memory for calloc()"));
		return NULL;
	}
	strcpy(new_dir->prefix, dir_prefix);

	/* link into the tree, sorted by prefix. */
	for (old_dir = root; old_dir; prev_dir = old_dir,
			old_dir = old_dir->next) {
		 result = strcmp(old_dir->prefix, dir_prefix);
		 if (result == 0) {
			free(new_dir);
			return old_dir;
		 } else if (result > 0)
			break;
	}
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
	for (int i = 0; i < 3; i++) {
		*last_1++ = *last_2++;
		while (*last_2 != '.')
			*last_1++ = *last_2++;
		last_1[0] = '\0';
		xosfile_create_dir(ro_filename_directory, 0);
	}
	return new_dir;
}
