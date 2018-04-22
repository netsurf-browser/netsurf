/*
 * Copyright 2018 Vincent Sanders <vince@netsurf-browser.org>
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

/**
 * \file
 * GTK accelerator support
 *
 */

#include <stdint.h>
#include <gtk/gtk.h>

#include "utils/errors.h"
#include "utils/hashtable.h"

#include "gtk/resources.h"
#include "gtk/accelerator.h"

/** The hash table used to store the accelerators */
static struct hash_table *accelerators_hash = NULL;

nserror nsgtk_accelerator_init(char **respaths)
{
	nserror ret;
	const uint8_t *data;
	size_t data_size;

	ret = nsgtk_data_from_resname("accelerators", &data, &data_size);
	if (ret == NSERROR_OK) {
		//ret = hashtable_add_from_inline(data, data_size);
	} else {
		const char *accelerators;
		/* Obtain path to accelerators */
		ret = nsgtk_path_from_resname("accelerators", &accelerators);
		if (ret == NSERROR_OK) {
			//ret = hashtable_add_from_file(messages);
		}
	}
	return ret;
}

const char *nsgtk_accelerator_get_desc(const char *key)
{
	return NULL;
}
