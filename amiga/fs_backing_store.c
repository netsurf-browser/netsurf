/*
 * Copyright 2014 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <proto/dos.h>
#include <proto/exec.h>

#include "content/fs_backing_store.c"

struct ami_backing_store_write {
	nsurl *url;
	enum backing_store_flags flags;
	uint8_t *data;
	size_t datalen;
};


static int32 ami_backing_store_write_process(STRPTR args, int32 length, APTR execbase)
{
	struct Process *proc = (struct Process *)FindTask(NULL);
	struct ami_backing_store_write *absw = proc->pr_Task.tc_UserData;

	filesystem_llcache_table->store(absw->url, absw->flags, absw->data, absw->datalen);
	FreeVec(absw);

	return RETURN_OK;
}


/**
 * Place an object in the backing store.
 * This implementation starts a new process and calls the core routine.
 *
 * @param url The url is used as the unique primary key for the data.
 * @param flags The flags to control how the object is stored.
 * @param data The objects source data.
 * @param datalen The length of the \a data.
 * @return NSERROR_OK on success or error code on faliure.
 */
static nserror
ami_backing_store_store(nsurl *url,
      enum backing_store_flags flags,
      const uint8_t *data,
      const size_t datalen)
{
	struct ami_backing_store_write *absw =
		AllocVecTagList(sizeof(struct ami_backing_store_write), NULL);

	if(absw == NULL) return NSERROR_NOMEM;

	absw->url = url;
	absw->flags = flags;
	absw->data = (uint8_t *)data;
	absw->datalen = datalen;

	struct Process *proc = CreateNewProcTags(
		NP_Name, "NetSurf backing store write process",
		NP_Entry, ami_backing_store_write_process,
		NP_Child, TRUE,
		NP_StackSize, 16384,
		NP_Priority, -1,
		NP_UserData, absw,
		TAG_DONE);

	if(proc == NULL) {
		FreeVec(absw);
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}


static struct gui_llcache_table amiga_llcache_table = {
	.initialise = initialise,
	.finalise = finalise,
	.store = ami_backing_store_store,
	.fetch = fetch,
	.invalidate = invalidate,
};

struct gui_llcache_table *amiga_filesystem_llcache_table = &amiga_llcache_table;

