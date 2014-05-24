/*
 * Copyright 2014 Vincent Sanders <vince@netsurf-browser.org>
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

/** \file
 * Low-level resource cache persistent storage implementation.
 *
 * file based backing store.
 *
 * \todo Consider improving eviction sorting to include objects size
 *         and remaining lifetime and other cost metrics.
 *
 * \todo make backing store have a more efficient small object storage.
 *
 */

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

#include "utils/filepath.h"
#include "utils/file.h"
#include "utils/nsurl.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "utils/messages.h"
#include "desktop/gui.h"

#include "content/backing_store.h"

/** Default number of bits of the ident to use in index hash */
#define DEFAULT_IDENT_SIZE 20

/** Default number of bits to use for an entry index. */
#define DEFAULT_ENTRY_SIZE 16

/** Backing store file format version */
#define CONTROL_VERSION 100

/** Get address from ident */
#define BS_ADDRESS(ident, state) ((ident) & ((1 << state->ident_bits) - 1))

/** Lookup store entry index from ident */
#define BS_ENTRY_INDEX(ident, state) state->addrmap[(ident) & ((1 << state->ident_bits) - 1)]

/** Get store entry from ident. */
#define BS_ENTRY(ident, state) state->entries[state->addrmap[(ident) & ((1 << state->ident_bits) - 1)]]

enum store_entry_flags {
	STORE_ENTRY_FLAG_NONE = 0,
};

/**
 * The type used to store index values refering to store entries. Care
 * must be taken with this type as it is used to build address to
 * entry mapping so changing the size will have large impacts on
 * memory usage.
 */
typedef uint16_t entry_index_t;

/**
 * The type used as a binary identifier for each entry derived from
 * the url. A larger identifier will have fewer collisions but
 * requires proportionately more storage.
 */
typedef uint32_t entry_ident_t;

/**
 * Backing store object index entry.
 *
 * @note Order is important to avoid structure packing overhead.
 */
struct store_entry {
	int64_t last_used; /**< unix time the entry was last used */
	entry_ident_t ident; /**< entry identifier */
	uint32_t data_alloc; /**< currently allocated size of data on disc */
	uint32_t meta_alloc; /**< currently allocated size of metadata on disc */
	uint16_t use_count; /**< number of times this entry has been accessed */
	uint16_t flags; /**< entry flags (unused) */
	uint16_t data_block; /**< small object data block entry (unused) */
	uint16_t meta_block; /**< small object meta block entry (unused) */
};

/**
 * Parameters controlling the backing store.
 */
struct store_state {
	char *path; /**< The path to the backing store */
	size_t limit; /**< The backing store upper bound target size */
	size_t hysteresis; /**< The hysteresis around the target size */

	unsigned int ident_bits; /**< log2 number of bits to use for address. */

	struct store_entry *entries; /**< store entries. */
	unsigned int entry_bits; /**< log2 number of bits in entry index. */
	unsigned int last_entry; /**< index of last usable entry. */

	/** flag indicating if the entries have been made persistant
	 * since they were last changed.
	 */
	bool entries_dirty;

	/** URL identifier to entry index mapping.
	 *
	 * This is an open coded index on the entries url field and
	 * provides a computationaly inexpensive way to go from the
	 * url to an entry.
	 */
	entry_index_t *addrmap;

	uint64_t total_alloc; /**< total size of all allocated storage. */

	size_t hit_count; /**< number of cache hits */
	uint64_t hit_size; /**< size of storage served */
	size_t miss_count; /**< number of cache misses */

};

/**
 * Global storage state.
 *
 * @todo Investigate if there is a way to have a context rather than
 * use a global.
 */
struct store_state *storestate;



/**
 * Remove a backing store entry from the entry table.
 *
 * This finds the store entry associated with the given key and
 * removes it from the table. The removed entry is returned but is
 * only valid until the next set_store_entry call.
 *
 * @param state The store state to use.
 * @param url The value used as the unique key to search entries for.
 * @param bse Pointer used to return value.
 * @return NSERROR_OK and bse updated on succes or NSERROR_NOT_FOUND
 *         if no entry coresponds to the url.
 */
static nserror
remove_store_entry(struct store_state *state,
		   entry_ident_t ident,
		   struct store_entry **bse)
{
	entry_index_t sei; /* store entry index */

	sei = BS_ENTRY_INDEX(ident, state);
	if (sei == 0) {
		LOG(("ident 0x%08x not in index", ident));
		return NSERROR_NOT_FOUND;
	}

	if (state->entries[sei].ident != ident) {
		/* entry ident did not match */
		LOG(("ident 0x%08x did not match entry index %d", ident, sei));
		return NSERROR_NOT_FOUND;
	}

	/* sei is entry to be removed, we swap it to the end of the
	 * table so there are no gaps and the returned entry is held
	 * in storage with reasonable lifetime.
	 */

	/* remove entry from map */
	BS_ENTRY_INDEX(ident, state) = 0;

	/* global allocation accounting  */
	state->total_alloc -= state->entries[sei].data_alloc;
	state->total_alloc -= state->entries[sei].meta_alloc;

	state->last_entry--;

	if (sei == state->last_entry) {
		/* the removed entry was the last one, how conveniant */
		*bse = &state->entries[sei];
	} else {
		/* need to swap entries */
		struct store_entry tent;

		tent = state->entries[sei];
		state->entries[sei] = state->entries[state->last_entry];
		state->entries[state->last_entry] = tent;

		/* update map for moved entry */
		BS_ENTRY_INDEX(state->entries[sei].ident, state) = sei;

		*bse = &state->entries[state->last_entry];
	}

	return NSERROR_OK;
}


/**
 * Generate a filename for an object.
 *
 * @param state The store state to use.
 * @param ident The identifier to use.
 * @return The filename string or NULL on allocation error.
 */
static char *
store_fname(struct store_state *state,
	    entry_ident_t ident,
	    enum backing_store_flags flags)
{
	char *fname = NULL;
	uint8_t b64u_i[7]; /* base64 ident */
	uint8_t b64u_d[6][2]; /* base64 ident as separate components */
	const char *dat;

	/** Base64url encoding table */
	static const uint8_t encoding_table[] = {
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
		'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
		'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
		'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
		'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
		'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
		'w', 'x', 'y', 'z', '0', '1', '2', '3',
		'4', '5', '6', '7', '8', '9', '-', '_'
	};

	/* base64 encode ident */
	b64u_i[0] = b64u_d[0][0] = encoding_table[(ident      ) & 0x3f];
	b64u_i[1] = b64u_d[1][0] = encoding_table[(ident >>  6) & 0x3f];
	b64u_i[2] = b64u_d[2][0] = encoding_table[(ident >> 12) & 0x3f];
	b64u_i[3] = b64u_d[3][0] = encoding_table[(ident >> 18) & 0x3f];
	b64u_i[4] = b64u_d[4][0] = encoding_table[(ident >> 24) & 0x3f];
	b64u_i[5] = b64u_d[5][0] = encoding_table[(ident >> 30) & 0x3f];
	/* null terminate strings */
	b64u_i[6] = b64u_d[0][1] = b64u_d[1][1] = b64u_d[2][1] =
		b64u_d[3][1] = b64u_d[4][1] = b64u_d[5][1] = 0;

	if ((flags & BACKING_STORE_META) != 0) {
		dat = "meta";
	} else {
		dat = "data";
	}

	/* number of chars with usefully encoded data in b64 */
	switch(((state->ident_bits + 5) / 6)) {
	case 1:
		netsurf_mkpath(&fname, NULL, 3,
			       state->path,
			       dat,
			       b64u_i);
		break;

	case 2:
		netsurf_mkpath(&fname, NULL, 4,
			       state->path,
			       dat,
			       b64u_d[0],
			       b64u_i);
		break;

	case 3:
		netsurf_mkpath(&fname, NULL, 5,
			       state->path,
			       dat,
			       b64u_d[0],
			       b64u_d[1],
			       b64u_i);
		break;

	case 4:
		netsurf_mkpath(&fname, NULL, 6,
			       state->path,
			       dat,
			       b64u_d[0],
			       b64u_d[1],
			       b64u_d[2],
			       b64u_i);
		break;

	case 5:
		netsurf_mkpath(&fname, NULL, 7,
			       state->path,
			       dat,
			       b64u_d[0],
			       b64u_d[1],
			       b64u_d[2],
			       b64u_d[3],
			       b64u_i);
		break;

	case 6:
		netsurf_mkpath(&fname, NULL, 8,
			       state->path,
			       dat,
			       b64u_d[0],
			       b64u_d[1],
			       b64u_d[2],
			       b64u_d[3],
			       b64u_d[4],
			       b64u_i);
		break;

	default:
		assert(false);
	}

	return fname;
}


/**
 * Remove the entry and files associated with an identifier.
 *
 * @param state The store state to use.
 * @param ident The identifier to use.
 * @return NSERROR_OK on sucess or error code on failure.
 */
static nserror
unlink_ident(struct store_state *state, entry_ident_t ident)
{
	char *fname;
	nserror ret;
	struct store_entry *bse;

	/* LOG(("ident %08x", ident)); */

	/* use the url hash as the entry identifier */
	ret = remove_store_entry(state, ident, &bse);
	if (ret != NSERROR_OK) {
		/* LOG(("entry not found")); */
		return ret;
	}

	fname = store_fname(state, bse->ident, BACKING_STORE_META);
	if (fname == NULL) {
		return NSERROR_NOMEM;
	}
	unlink(fname);
	free(fname);

	fname = store_fname(state, bse->ident, BACKING_STORE_NONE);
	if (fname == NULL) {
		return NSERROR_NOMEM;
	}
	unlink(fname);
	free(fname);

	return NSERROR_OK;
}


/**
 * Quick sort comparison.
 */
static int compar(const void *va, const void *vb)
{
	const struct store_entry *a = &BS_ENTRY(*(entry_ident_t *)va, storestate);
	const struct store_entry *b = &BS_ENTRY(*(entry_ident_t *)vb, storestate);

	if (a->use_count < b->use_count) {
		return -1;
	} else if (a->use_count > b->use_count) {
		return 1;
	}
	/* use count is the same - now consider last use time */

	if (a->last_used < b->last_used) {
		return -1;
	} else if (a->last_used > b->last_used) {
		return 1;
	}

	/* they are the same */
	return 0;
}


/**
 * Evict entries from backing store as per configuration.
 *
 * Entries are evicted to ensure the cache remains within the
 * configured limits on size and number of entries.
 *
 * The approach is to check if the cache limits have been exceeded and
 * if so build and sort list of entries to evict. The list is sorted
 * by use count and then by age, so oldest object with least number of uses
 * get evicted first.
 *
 * @param state The store state to use.
 * @return NSERROR_OK on success or error code on failure.
 */
static nserror store_evict(struct store_state *state)
{
	entry_ident_t *elist; /* sorted list of entry identifiers */
	unsigned int ent;
	unsigned int ent_count;
	size_t removed; /* size of removed entries */
	nserror ret = NSERROR_OK;

	/* check if the cache has exceeded configured limit */
	if ((state->total_alloc < state->limit) &&
	    (state->last_entry < (1U << state->entry_bits))) {
		/* cache within limits */
		return NSERROR_OK;
	}

	LOG(("Evicting entries to reduce %d by %d",
	     state->total_alloc, state->hysteresis));

	/* allocate storage for the list */
	elist = malloc(sizeof(entry_ident_t) * state->last_entry);
	if (elist == NULL) {
		return NSERROR_NOMEM;
	}

	/* sort the list avoiding entry 0 which is the empty sentinel */
	for (ent = 1; ent < state->last_entry; ent++) {
		elist[ent - 1] = state->entries[ent].ident;
	}
	ent_count = ent - 1; /* important to keep this as the entry count will change when entries are removed */
	qsort(elist, ent_count, sizeof(entry_ident_t), compar);

	/* evict entries in listed order */
	removed = 0;
	for (ent = 0; ent < ent_count; ent++) {

		removed += BS_ENTRY(elist[ent], state).data_alloc;
		removed += BS_ENTRY(elist[ent], state).meta_alloc;

		ret = unlink_ident(state, elist[ent]);
		if (ret != NSERROR_OK) {
			break;
		}

		if (removed > state->hysteresis) {
			break;
		}
	}

	free(elist);

	LOG(("removed %d in %d entries", removed, ent));

	return ret;
}


/**
 * Lookup a backing store entry in the entry table from a url.
 *
 * This finds the store entry associated with the given
 * key. Additionally if an entry is found it updates the usage data
 * about the entry.
 *
 * @param state The store state to use.
 * @param url The value used as the unique key to search entries for.
 * @param bse Pointer used to return value.
 * @return NSERROR_OK and bse updated on success or NSERROR_NOT_FOUND
 *         if no entry corresponds to the url.
 */
static nserror
get_store_entry(struct store_state *state, nsurl *url, struct store_entry **bse)
{
	entry_ident_t ident;
	unsigned int sei; /* store entry index */

	LOG(("url:%s", nsurl_access(url)));

	/* use the url hash as the entry identifier */
	ident = nsurl_hash(url);

	sei = BS_ENTRY_INDEX(ident, state);

	if (sei == 0) {
		return NSERROR_NOT_FOUND;
	}

	if (state->entries[sei].ident != ident) {
		/* entry ident did not match */
		LOG(("ident did not match entry"));
		return NSERROR_NOT_FOUND;
	}

	*bse = &state->entries[sei];

	state->entries[sei].last_used = time(NULL);
	state->entries[sei].use_count++;

	state->entries_dirty = true;

	return NSERROR_OK;
}


/**
 * Set a backing store entry in the entry table from a url.
 *
 * This creates a backing store entry in the entry table for a url.
 *
 * @param url The value used as the unique key to search entries for.
 * @param bse Pointer used to return value.
 * @return NSERROR_OK and bse updated on succes or NSERROR_NOT_FOUND
 *         if no entry coresponds to the url.
 */
static nserror
set_store_entry(struct store_state *state,
		nsurl *url,
		enum backing_store_flags flags,
		const uint8_t *data,
		const size_t datalen,
		struct store_entry **bse)
{
	entry_ident_t ident;
	entry_index_t sei; /* store entry index */
	struct store_entry *se;
	nserror ret;
	bool isrep; /* is the store repalcing an existing entry or not */

	LOG(("url:%s", nsurl_access(url)));

	/* evict entries as required and ensure there is at least one
	 * new entry available.
	 */
	ret = store_evict(state);
	if (ret != NSERROR_OK) {
		return ret;
	}

	/* use the url hash as the entry identifier */
	ident = nsurl_hash(url);

	sei = BS_ENTRY_INDEX(ident, state);

	/** @todo Should this deal with cache eviction? */

	if (sei == 0) {
		/* allocating the next available entry */
		sei = state->last_entry;
		state->last_entry++;
		BS_ENTRY_INDEX(ident, state) = sei;
		isrep = false;
	} else {
		/* updating or replacing existing entry */
		/** @todo should we be checking the entry ident
		 * matches the url. Thats a collision in the address
		 * mapping right? and is it important?
		 */
		isrep = true;
	}

	se = &state->entries[sei];

	se->ident = ident;
	se->flags = STORE_ENTRY_FLAG_NONE;
	se->use_count = 1;
	se->last_used = time(NULL);

	/* account for allocation */
	if ((flags & BACKING_STORE_META) != 0) {
		if (isrep) {
			state->total_alloc -= se->meta_alloc;
		} else {
			se->data_alloc = 0;
		}
		se->meta_alloc = datalen;
	} else {
		if (isrep) {
			state->total_alloc -= se->data_alloc;
		} else {
			se->meta_alloc = 0;
		}
		se->data_alloc = datalen;
	}
	state->total_alloc += datalen;

	state->entries_dirty = true;

	*bse = se;

	return NSERROR_OK;
}




/**
 * Open a file using a store ident.
 *
 * @param state The store state to use.
 * @param ident The identifier of the file to open.
 * @param flags The backing store flags.
 * @pram openflags The flags used with the open call.
 * @return An fd from the open call or -1 on error.
 */
static int
store_open(struct store_state *state,
	   uint32_t ident,
	   enum backing_store_flags flags,
	   int openflags)
{
	char *fname;
	nserror ret;
	int fd;

	fname = store_fname(state, ident, flags);
	if (fname == NULL) {
		LOG(("filename error"));
		return -1;
	}

	/* ensure path to file is usable */
	ret = filepath_mkdir_all(fname);
	if (ret != NSERROR_OK) {
		LOG(("file path \"%s\" could not be created", fname));
		free(fname);
		return -1;
	}

	LOG(("opening %s", fname));
	fd = open(fname, openflags, S_IRUSR | S_IWUSR);

	free(fname);

	return fd;
}

/**
 * Construct address ident to filesystem entry map
 *
 * To allow a filesystem entry to be found from it's identifier we
 * construct an mapping index. This is a hash map from the entries URL
 * (its unique key) to filesystem entry.
 *
 * As the entire entry list must be iterated over to construct the map
 * we also compute the total storage in use.
 *
 * @param state The backing store global state.
 * @return NSERROR_OK on sucess or NSERROR_NOMEM if the map storage
 *         could not be allocated.
 */
static nserror
build_entrymap(struct store_state *state)
{
	unsigned int eloop;

	LOG(("Allocating %d bytes for max of %d buckets",
	     (1 << state->ident_bits) * sizeof(entry_index_t),
	     1 << state->ident_bits));

	state->addrmap = calloc(1 << state->ident_bits, sizeof(entry_index_t));
	if (state->addrmap == NULL) {
		return NSERROR_NOMEM;
	}

	state->total_alloc = 0;

	for (eloop = 1; eloop < state->last_entry; eloop++) {
		/*
		LOG(("entry:%d ident:0x%08x used:%d",
		     eloop,
		     BS_ADDRESS(state->entries[eloop].ident, state),
		     state->entries[eloop].use_count));
		*/

		/* update the address map to point at the entry */
		BS_ENTRY_INDEX(state->entries[eloop].ident, state) = eloop;

		/* account for the storage space */
		state->total_alloc += state->entries[eloop].data_alloc +
			state->entries[eloop].meta_alloc;
	}

	return NSERROR_OK;
}

/**
 * Write filesystem entries to file.
 *
 * @param state The backing store state to read the entries from.
 * @return NSERROR_OK on sucess or error code on faliure.
 */
static nserror write_entries(struct store_state *state)
{
	int fd;
	char *fname = NULL;
	ssize_t written;
	nserror ret;

	if (state->entries_dirty == false) {
		/* entries have not been updated since last write */
		return NSERROR_OK;
	}

	ret = netsurf_mkpath(&fname, NULL, 2, state->path, "entries");
	if (ret != NSERROR_OK) {
		return ret;
	}

	fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	free(fname);
	if (fd == -1) {
		return NSERROR_SAVE_FAILED;
	}

	written = write(fd, state->entries,
			state->last_entry * sizeof(struct store_entry));
	close(fd);
	if (written < 0) {
		/* TODO: Delete the file? */
		return NSERROR_SAVE_FAILED;
	}

	return NSERROR_OK;
}

/**
 * Read description entries into memory.
 *
 * @param state The backing store state to put the loaded entries in.
 * @return NSERROR_OK on sucess or error code on faliure.
 */
static nserror
read_entries(struct store_state *state)
{
	int fd;
	ssize_t rd;
	size_t entries_size;
	char *fname = NULL;
	nserror ret;

	ret = netsurf_mkpath(&fname, NULL, 2, state->path, "entries");
	if (ret != NSERROR_OK) {
		return ret;
	}

	entries_size = (1 << state->entry_bits) * sizeof(struct store_entry);

	LOG(("Allocating %d bytes for max of %d entries",
	     entries_size, 1 << state->entry_bits));

	state->entries = calloc(1, entries_size);
	if (state->entries == NULL) {
		free(fname);
		return NSERROR_NOMEM;
	}

	fd = open(fname, O_RDWR);
	free(fname);
	if (fd != -1) {
		rd = read(fd, state->entries, entries_size);
		close(fd);
		if (rd > 0) {
			state->last_entry = rd / sizeof(struct store_entry);
			LOG(("Read %d entries", state->last_entry));
		}
	} else {
		/* could rebuild entries from fs */
		state->last_entry = 1;
	}
	return NSERROR_OK;
}


/**
 * Write the control file for the current state.
 *
 * @param state The state to write to the control file.
 * @return NSERROR_OK on sucess or error code on faliure.
 */
static nserror
write_control(struct store_state *state)
{
	FILE *fcontrol;
	nserror ret;
	char *fname = NULL;

	ret = netsurf_mkpath(&fname, NULL, 2, state->path, "control");
	if (ret != NSERROR_OK) {
		return ret;
	}

	ret = filepath_mkdir_all(fname);
	if (ret != NSERROR_OK) {
		free(fname);
		return ret;
	}

	fcontrol = fopen(fname, "wb");

	free(fname);

	if (fcontrol == NULL) {
		return NSERROR_NOT_FOUND;
	}

	fprintf(fcontrol, "%u%c", CONTROL_VERSION, 0);
	fprintf(fcontrol, "%u%c", state->entry_bits, 0);
	fprintf(fcontrol, "%u%c", state->ident_bits, 0);
	fprintf(fcontrol, "%u%c", state->last_entry, 0);

	fclose(fcontrol);

	return NSERROR_OK;
}


/**
 * Read and parse the control file.
 *
 * @param state The state to read from the control file.
 * @return NSERROR_OK on sucess or error code on faliure.
 */
static nserror
read_control(struct store_state *state)
{
	nserror ret;
	FILE *fcontrol;
	unsigned int ctrlversion;
	unsigned int addrbits;
	unsigned int entrybits;
	char *fname = NULL;

	ret = netsurf_mkpath(&fname, NULL, 2, state->path, "control");
	if (ret != NSERROR_OK) {
		return ret;
	}

	fcontrol = fopen(fname, "rb");

	free(fname);

	if (fcontrol == NULL) {
		/* unable to open control file */
		if (errno == ENOENT) {
			return NSERROR_NOT_FOUND;
		} else {
			return NSERROR_INIT_FAILED;
		}
	}

	/* read control and setup new state */

	/* first line is version */
	if (fscanf(fcontrol, "%u", &ctrlversion) != 1) {
		goto control_error;
	}

	if (ctrlversion != CONTROL_VERSION) {
		goto control_error;
	}

	if (fgetc(fcontrol) != 0) {
		goto control_error;
	}

	/* second line is log2 max number of entries */
	if (fscanf(fcontrol, "%u", &entrybits) != 1) {
		LOG(("c"));
		goto control_error;
	}
	if (fgetc(fcontrol) != 0) {
		goto control_error;
	}

	/* second line is log2 size of address hash */
	if (fscanf(fcontrol, "%u", &addrbits) != 1) {
		LOG(("d"));
		goto control_error;
	}
	if (fgetc(fcontrol) != 0) {
		goto control_error;
	}

	fclose(fcontrol);

	state->entry_bits = entrybits;
	state->ident_bits = addrbits;

	return NSERROR_OK;

control_error: /* problem with the control file */

	fclose(fcontrol);

	return NSERROR_INIT_FAILED;
}




/* Functions exported in the backing store table */

/**
 * Initialise the backing store.
 *
 * @param parameters to configure backing store.
 * @return NSERROR_OK on success or error code on faliure.
 */
static nserror
initialise(const struct llcache_store_parameters *parameters)
{
	struct store_state *newstate;
	nserror ret;

	/* check backing store is not already initialised */
	if (storestate != NULL) {
		return NSERROR_INIT_FAILED;
	}

	/* if we are not allowed any space simply give up on init */
	if (parameters->limit == 0) {
		return NSERROR_OK;
	}

	/* if the path to the cache directory is not set do not init */
	if (parameters->path == NULL) {
		return NSERROR_OK;
	}

	/* allocate new store state and set defaults */
	newstate = calloc(1, sizeof(struct store_state));
	if (newstate == NULL) {
		return NSERROR_NOMEM;
	}

	newstate->path = strdup(parameters->path);
	newstate->limit = parameters->limit;
	newstate->hysteresis = parameters->hysteresis;

	if (parameters->address_size == 0) {
		newstate->ident_bits = DEFAULT_IDENT_SIZE;
	} else {
		newstate->ident_bits = parameters->address_size;
	}

	if (parameters->entry_size == 0) {
		newstate->entry_bits = DEFAULT_ENTRY_SIZE;
	} else {
		newstate->entry_bits = parameters->entry_size;
	}

	ret = read_control(newstate);
	if (ret != NSERROR_OK) {
		LOG(("read control failed %s", messages_get_errorcode(ret)));
		ret = write_control(newstate);
	}
	if (ret != NSERROR_OK) {
		/* that went well obviously */
		free(newstate->path);
		free(newstate);
		return ret;
	}

	/* ensure the maximum number of entries can be represented in
	 * the type available to store it.
	 */
	if (newstate->entry_bits > (8 * sizeof(entry_index_t))) {
		newstate->entry_bits = (8 * sizeof(entry_index_t));
	}

	/* read filesystem entries */
	ret = read_entries(newstate);
	if (ret != NSERROR_OK) {
		/* that went well obviously */
		free(newstate->path);
		free(newstate);
		return ret;
	}

	/* build entry hash map */
	ret = build_entrymap(newstate);
	if (ret != NSERROR_OK) {
		/* that obviously went well  */
		free(newstate->path);
		free(newstate);
		return ret;
	}

	storestate = newstate;

	LOG(("FS backing store init successful"));

	LOG(("path:%s limit:%d hyst:%d addr:%d entries:%d", newstate->path, newstate->limit, newstate->hysteresis, newstate->ident_bits, newstate->entry_bits));
	LOG(("Using %d/%d", newstate->total_alloc, newstate->limit));

	return NSERROR_OK;
}


/**
 * Finalise the backing store.
 *
 * @return NSERROR_OK on success.
 */
static nserror
finalise(void)
{
	if (storestate != NULL) {
		write_entries(storestate);

		/* avoid division by zero */
		if (storestate->miss_count == 0) {
			storestate->miss_count = 1;
		}
		LOG(("hits:%d misses:%d hit ratio:%d returned:%d bytes",
		     storestate->hit_count, storestate->miss_count,
		     storestate->hit_count / storestate->miss_count,
		     storestate->hit_size));

		free(storestate->path);
		free(storestate);
		storestate = NULL;
	}
	return NSERROR_OK;
}


/**
 * Place an object in the backing store.
 *
 * @param url The url is used as the unique primary key for the data.
 * @param flags The flags to control how the object is stored.
 * @param data The objects source data.
 * @param datalen The length of the \a data.
 * @return NSERROR_OK on success or error code on faliure.
 */
static nserror
store(nsurl *url,
      enum backing_store_flags flags,
      const uint8_t *data,
      const size_t datalen)
{
	nserror ret;
	struct store_entry *bse;
	ssize_t written;
	int fd;

	/* check backing store is initialised */
	if (storestate == NULL) {
		return NSERROR_INIT_FAILED;
	}

	/* set the store entry up */
	ret = set_store_entry(storestate, url, flags, data, datalen, &bse);
	if (ret != NSERROR_OK) {
		LOG(("store entry setting failed"));
		return ret;
	}

	fd = store_open(storestate, bse->ident, flags, O_CREAT | O_WRONLY);
	if (fd < 0) {
		perror("");
		LOG(("Open failed %d",fd));
		return NSERROR_SAVE_FAILED;
	}


	LOG(("Writing %d bytes from %p", datalen, data));
	written = write(fd, data, datalen);

	close(fd);
	if (written < 0) {
		/* TODO: Delete the file? */
		return NSERROR_SAVE_FAILED;
	}

	return NSERROR_OK;
}

/**
 * Retrive an object from the backing store.
 *
 * @param url The url is used as the unique primary key for the data.
 * @param flags The flags to control how the object is stored.
 * @param data The objects data.
 * @param datalen The length of the \a data retrieved.
 * @return NSERROR_OK on success or error code on faliure.
 */
static nserror
fetch(nsurl *url,
      enum backing_store_flags *flags,
      uint8_t **data_out,
      size_t *datalen_out)
{
	nserror ret;
	struct store_entry *bse;
	uint8_t *data;
	size_t datalen;
	int fd;
	ssize_t rd;

	/* check backing store is initialised */
	if (storestate == NULL) {
		return NSERROR_INIT_FAILED;
	}

	ret = get_store_entry(storestate, url, &bse);
	if (ret != NSERROR_OK) {
		LOG(("entry not found"));
		storestate->miss_count++;
		return ret;
	}
	storestate->hit_count++;

	LOG(("retriving cache file for url:%s", nsurl_access(url)));

	fd = store_open(storestate, bse->ident, *flags, O_RDONLY);
	if (fd < 0) {
		LOG(("Open failed"));
		/** @todo should this invalidate the entry? */
		return NSERROR_NOT_FOUND;
	}

	data = *data_out;
	datalen = *datalen_out;

	/* need to deal with buffers */
	if (data == NULL) {
		if (datalen == 0) {
			/* caller did not know the files length */
			if (((*flags) & BACKING_STORE_META) != 0) {
				datalen = bse->meta_alloc;
			} else {
				datalen = bse->data_alloc;
			}
		}

		data = malloc(datalen);
		if (data == NULL) {
			close(fd);
			return NSERROR_NOMEM;
		}
	}

	/** @todo should this check datalen is sufficient */

	LOG(("Reading %d bytes into %p from file", datalen, data));

	/** @todo this read should be an a loop */
	rd = read(fd, data, datalen);
	if (rd <= 0) {
		LOG(("read returned %d", rd));
		close(fd);
		if ((*data_out) == NULL) {
			free(data);
		}
		return NSERROR_NOT_FOUND;
	}

	close(fd);

	storestate->hit_size += datalen;

	*data_out = data;
	*datalen_out = datalen;

	return NSERROR_OK;
}


/**
 * Invalidate a source object from the backing store.
 *
 * The entry (if present in the backing store) must no longer
 * be returned as a result to the fetch or meta operations.
 *
 * @param url The url is used as the unique primary key to invalidate.
 * @return NSERROR_OK on success or error code on faliure.
 */
static nserror
invalidate(nsurl *url)
{
	/* check backing store is initialised */
	if (storestate == NULL) {
		return NSERROR_INIT_FAILED;
	}

	LOG(("url:%s", nsurl_access(url)));

	return unlink_ident(storestate, nsurl_hash(url));
}


static struct gui_llcache_table llcache_table = {
	.initialise = initialise,
	.finalise = finalise,
	.store = store,
	.fetch = fetch,
	.invalidate = invalidate,
};

struct gui_llcache_table *filesystem_llcache_table = &llcache_table;
