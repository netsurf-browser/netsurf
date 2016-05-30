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

/**
 * \file
 * Low-level resource cache persistent storage implementation.
 *
 * file based backing store.
 *
 * \todo Consider improving eviction sorting to include objects size
 *         and remaining lifetime and other cost metrics.
 *
 * \todo Implement mmap retrieval where supported.
 *
 * \todo Implement static retrival for metadata objects as their heap
 *         lifetime is typically very short, though this may be obsoleted
 *         by a small object storage stratagy.
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
#include <nsutils/unistd.h>

#include "utils/filepath.h"
#include "utils/file.h"
#include "utils/nsurl.h"
#include "utils/log.h"
#include "utils/utils.h"
#include "utils/messages.h"
#include "desktop/gui_internal.h"
#include "netsurf/misc.h"

#include "content/backing_store.h"

/** Default number of bits of the ident to use in index hash */
#define DEFAULT_IDENT_SIZE 20

/** Default number of bits to use for an entry index. */
#define DEFAULT_ENTRY_SIZE 16

/** Backing store file format version */
#define CONTROL_VERSION 130

/** Number of milliseconds after a update before control data maintinance is performed  */
#define CONTROL_MAINT_TIME 10000

/** Get address from ident */
#define BS_ADDRESS(ident, state) ((ident) & ((1 << state->ident_bits) - 1))

/** Lookup store entry index from ident */
#define BS_ENTRY_INDEX(ident, state) state->addrmap[(ident) & ((1 << state->ident_bits) - 1)]

/** Get store entry from ident. */
#define BS_ENTRY(ident, state) state->entries[state->addrmap[(ident) & ((1 << state->ident_bits) - 1)]]

/** Filename of serialised entries */
#define ENTRIES_FNAME "entries"

/** Filename of block file index */
#define BLOCKS_FNAME "blocks"

/** log2 block data address length (64k) */
#define BLOCK_ADDR_LEN 16

/** log2 number of entries per block file(4k) */
#define BLOCK_ENTRY_COUNT 12

/** log2 number of data block files */
#define BLOCK_FILE_COUNT (BLOCK_ADDR_LEN - BLOCK_ENTRY_COUNT)

/** log2 size of data blocks (8k) */
#define BLOCK_DATA_SIZE 13

/** log2 size of metadata blocks (1k) */
#define BLOCK_META_SIZE 10

/** length in bytes of a block files use map */
#define BLOCK_USE_MAP_SIZE (1 << (BLOCK_ENTRY_COUNT - 3))

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
 * The type used to store block file index values. If this is changed
 * it will affect the entry storage/alignment and BLOCK_ADDR_LEN must
 * also be updated.
 */
typedef uint16_t block_index_t;


/**
 * Entry element index values.
 */
enum store_entry_elem_idx {
	ENTRY_ELEM_DATA = 0, /**< entry element is data */
	ENTRY_ELEM_META = 1,  /**< entry element is metadata */
	ENTRY_ELEM_COUNT = 2, /**< count of elements on an entry */
};

/**
 * flags that indicate what additional information is contained within
 * an entry element.
 */
enum store_entry_elem_flags {
	/** store not managing any allocation on entry */
	ENTRY_ELEM_FLAG_NONE = 0,
	/** entry data allocation is on heap */
	ENTRY_ELEM_FLAG_HEAP = 0x1,
	/** entry data allocation is mmaped */
	ENTRY_ELEM_FLAG_MMAP = 0x2,
	/** entry data allocation is in small object pool */
	ENTRY_ELEM_FLAG_SMALL = 0x4,
};


enum store_entry_flags {
	/** entry is normal */
	ENTRY_FLAGS_NONE = 0,
	/** entry has been invalidated but something still holding a reference */
	ENTRY_FLAGS_INVALID = 1,
};

/**
 * Backing store entry element.
 *
 * An element keeps data about:
 *  - the current memory allocation
 *  - the number of outstanding references to the memory
 *  - the size of the element data
 *  - flags controlling how the memory and element are handled
 *
 * @note Order is important to avoid excessive structure packing overhead.
 */
struct store_entry_element {
	uint8_t* data; /**< data allocated */
	uint32_t size; /**< size of entry element on disc */
	block_index_t block; /**< small object data block */
	uint8_t ref; /**< element data reference count */
	uint8_t flags; /**< entry flags */
};

/**
 * Backing store object index entry.
 *
 * An entry in the backing store contains two elements for the actual
 * data and the metadata. The two elements are treated identically for
 * storage lifetime but as a collective whole for expiration and
 * indexing.
 *
 * @note Order is important to avoid excessive structure packing overhead.
 */
struct store_entry {
	int64_t last_used; /**< unix time the entry was last used */
	entry_ident_t ident; /**< entry identifier */
	uint16_t use_count; /**< number of times this entry has been accessed */
	uint8_t flags; /**< entry flags */
	/** Entry element (data or meta) specific information */
	struct store_entry_element elem[ENTRY_ELEM_COUNT];
};

/**
 * Small block file.
 */
struct block_file {
	/** file descriptor of the block file */
	int fd;
	/** map of used and unused entries within the block file */
	uint8_t use_map[BLOCK_USE_MAP_SIZE];
};

/**
 * log2 of block size.
 */
static const unsigned int log2_block_size[ENTRY_ELEM_COUNT] = {
	BLOCK_DATA_SIZE, /**< Data block size */
	BLOCK_META_SIZE  /**< Metadata block size */
};

/**
 * Parameters controlling the backing store.
 */
struct store_state {
	/* store config */
	char *path; /**< The path to the backing store */
	size_t limit; /**< The backing store upper bound target size */
	size_t hysteresis; /**< The hysteresis around the target size */

	unsigned int ident_bits; /**< log2 number of bits to use for address. */


	/* cache entry management */
	struct store_entry *entries; /**< store entries. */
	unsigned int entry_bits; /**< log2 number of bits in entry index. */
	unsigned int last_entry; /**< index of last usable entry. */

	/** flag indicating if the entries have been made persistant
	 * since they were last changed.
	 */
	bool entries_dirty;

	/**
	 * URL identifier to entry index mapping.
	 *
	 * This is an open coded index on the entries url field and
	 * provides a computationaly inexpensive way to go from the
	 * url to an entry.
	 */
	entry_index_t *addrmap;


	/** small block indexes */
	struct block_file blocks[ENTRY_ELEM_COUNT][BLOCK_FILE_COUNT];

	/** flag indicating if the block file use maps have been made
	 * persistant since they were last changed.
	 */
	bool blocks_dirty;

	/** flag indicating if a block file has been opened for update
	 * since maintinance was previously done.
	 */
	bool blocks_opened;


	/* stats */
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
 * @param[in] state The store state to use.
 * @param[in, out] bse Pointer to the entry to be removed.
 * @return NSERROR_OK and \a bse updated on succes or NSERROR_NOT_FOUND
 *         if no entry coresponds to the url.
 */
static nserror
remove_store_entry(struct store_state *state, struct store_entry **bse)
{
	entry_index_t sei; /* store entry index */

	/* sei is index to entry to be removed, we swap it to the end
	 * of the table so there are no gaps and the returned entry is
	 * held in storage with reasonable lifetime.
	 */

	sei = BS_ENTRY_INDEX((*bse)->ident, state);

	/* remove entry from map */
	BS_ENTRY_INDEX((*bse)->ident, state) = 0;

	/* global allocation accounting  */
	state->total_alloc -= state->entries[sei].elem[ENTRY_ELEM_DATA].size;
	state->total_alloc -= state->entries[sei].elem[ENTRY_ELEM_META].size;

	state->last_entry--;

	if (sei == state->last_entry) {
		/* the removed entry was the last one, how convenient */
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
 * this generates the filename for an object on disc. It is necessary
 * for this to generate a filename which conforms to the limitations
 * of all the filesystems the cache can be placed upon.
 *
 * From http://en.wikipedia.org/wiki/Comparison_of_file_systems#Limits
 * the relevant subset is:
 *  - path elements no longer than 8 characters
 *  - acceptable characters are A-Z, 0-9
 *  - short total path lengths (255 or less)
 *  - no more than 77 entries per directory (6bits worth)
 *
 * The short total path lengths mean the encoding must represent as
 * much data as possible in the least number of characters.
 *
 * To achieve all these goals we use RFC4648 base32 encoding which
 * packs 5bits into each character of the filename. To represent a 32
 * bit ident this requires a total path length of between 17 and 22
 * bytes (including directory separators) BA/BB/BC/BD/BE/ABCDEFG
 *
 * @note Version 1.00 of the cache implementation used base64 to
 * encode this, however that did not meet the requirement for only
 * using uppercase characters.
 *
 * @note Versions prior to 1.30 only packed 5 bits per directory level
 * A/B/C/D/E/F/ABCDEFG which only required 19 characters to represent
 * but resulted in requiring an extra level of directory which is less
 * desirable than the three extra characters using six bits.
 *
 * @param state The store state to use.
 * @param ident The identifier to use.
 * @param elem_idx The element index.
 * @return The filename string or NULL on allocation error.
 */
static char *
store_fname(struct store_state *state,
	    entry_ident_t ident,
	    int elem_idx)
{
	char *fname = NULL;
	uint8_t b32u_i[8]; /* base32 encoded ident */
	const uint8_t *b32u_d[6]; /* base32 ident as separate components */

	/* directories used to separate elements */
	const char *base_dir_table[] = {
		"d", "m", "dblk", "mblk"
	};

	/* RFC4648 base32 encoding table (six bits) */
	const uint8_t encoding_table[64][3] = {
		{ 'A',   0, 0 }, { 'B',   0, 0 }, /*  0 */
		{ 'C',   0, 0 }, { 'D',   0, 0 }, /*  2 */
		{ 'E',   0, 0 }, { 'F',   0, 0 }, /*  4 */
		{ 'G',   0, 0 }, { 'H',   0, 0 }, /*  6 */
		{ 'I',   0, 0 }, { 'J',   0, 0 }, /*  8 */
		{ 'K',   0, 0 }, { 'L',   0, 0 }, /* 10 */
		{ 'M',   0, 0 }, { 'N',   0, 0 }, /* 12 */
		{ 'O',   0, 0 }, { 'P',   0, 0 }, /* 14 */
		{ 'Q',   0, 0 }, { 'R',   0, 0 }, /* 16 */
		{ 'S',   0, 0 }, { 'T',   0, 0 }, /* 18 */
		{ 'U',   0, 0 }, { 'V',   0, 0 }, /* 20 */
		{ 'W',   0, 0 }, { 'X',   0, 0 }, /* 22 */
		{ 'Y',   0, 0 }, { 'Z',   0, 0 }, /* 24 */
		{ '2',   0, 0 }, { '3',   0, 0 }, /* 26 */
		{ '4',   0, 0 }, { '5',   0, 0 }, /* 28 */
		{ '6',   0, 0 }, { '7',   0, 0 }, /* 30 */
		{ 'B', 'A', 0 }, { 'B', 'B', 0 }, /* 32 */
		{ 'B', 'C', 0 }, { 'B', 'D', 0 }, /* 34 */
		{ 'B', 'E', 0 }, { 'B', 'F', 0 }, /* 36 */
		{ 'B', 'G', 0 }, { 'B', 'H', 0 }, /* 38 */
		{ 'B', 'I', 0 }, { 'B', 'J', 0 }, /* 40 */
		{ 'B', 'K', 0 }, { 'B', 'L', 0 }, /* 42 */
		{ 'B', 'M', 0 }, { 'B', 'N', 0 }, /* 44 */
		{ 'B', 'O', 0 }, { 'B', 'P', 0 }, /* 46 */
		{ 'B', 'Q', 0 }, { 'B', 'R', 0 }, /* 48 */
		{ 'B', 'S', 0 }, { 'B', 'T', 0 }, /* 50 */
		{ 'B', 'U', 0 }, { 'B', 'V', 0 }, /* 52 */
		{ 'B', 'W', 0 }, { 'B', 'X', 0 }, /* 54 */
		{ 'B', 'Y', 0 }, { 'B', 'Z', 0 }, /* 56 */
		{ 'B', '2', 0 }, { 'B', '3', 0 }, /* 58 */
		{ 'B', '4', 0 }, { 'B', '5', 0 }, /* 60 */
		{ 'B', '6', 0 }, { 'B', '7', 0 }  /* 62 */
	};

	/* base32 encode ident */
	b32u_i[0] = encoding_table[(ident      ) & 0x1f][0];
	b32u_i[1] = encoding_table[(ident >>  5) & 0x1f][0];
	b32u_i[2] = encoding_table[(ident >> 10) & 0x1f][0];
	b32u_i[3] = encoding_table[(ident >> 15) & 0x1f][0];
	b32u_i[4] = encoding_table[(ident >> 20) & 0x1f][0];
	b32u_i[5] = encoding_table[(ident >> 25) & 0x1f][0];
	b32u_i[6] = encoding_table[(ident >> 30) & 0x1f][0];
	b32u_i[7] = 0; /* null terminate ident string */

	/* base32 encode directory separators */
	b32u_d[0] = (uint8_t*)base_dir_table[elem_idx];
	b32u_d[1] = &encoding_table[(ident      ) & 0x3f][0];
	b32u_d[2] = &encoding_table[(ident >>  6) & 0x3f][0];
	b32u_d[3] = &encoding_table[(ident >> 12) & 0x3f][0];
	b32u_d[4] = &encoding_table[(ident >> 18) & 0x3f][0];
	b32u_d[5] = &encoding_table[(ident >> 24) & 0x3f][0];

	switch (elem_idx) {
	case ENTRY_ELEM_DATA:
	case ENTRY_ELEM_META:
		netsurf_mkpath(&fname, NULL, 8,
			       state->path, b32u_d[0], b32u_d[1], b32u_d[2],
			       b32u_d[3], b32u_d[4], b32u_d[5], b32u_i);
		break;

	case (ENTRY_ELEM_COUNT + ENTRY_ELEM_META):
	case (ENTRY_ELEM_COUNT + ENTRY_ELEM_DATA):
		netsurf_mkpath(&fname, NULL, 3,
			       state->path, b32u_d[0], b32u_d[1]);
		break;

	default:
		assert("bad element index" == NULL);
		break;
	}

	return fname;
}

/**
 * invalidate an element of an entry
 *
 * @param state The store state to use.
 * @param bse The entry to invalidate.
 * @param elem_idx The element index to invalidate.
 * @return NSERROR_OK on sucess or error code on failure.
 */
static nserror
invalidate_element(struct store_state *state,
		   struct store_entry *bse,
		   int elem_idx)
{
	if (bse->elem[elem_idx].block != 0) {
		block_index_t bf;
		block_index_t bi;

		/* block file block resides in */
		bf = (bse->elem[elem_idx].block >> BLOCK_ENTRY_COUNT) &
			((1 << BLOCK_FILE_COUNT) - 1);

		/* block index in file */
		bi = bse->elem[elem_idx].block & ((1U << BLOCK_ENTRY_COUNT) -1);

		/* clear bit in use map */
		state->blocks[elem_idx][bf].use_map[bi >> 3] &= ~(1U << (bi & 7));
	} else {
		char *fname;

		/* unlink the file from disc */
		fname = store_fname(state, bse->ident, elem_idx);
		if (fname == NULL) {
			return NSERROR_NOMEM;
		}
		unlink(fname);
		free(fname);
	}

	return NSERROR_OK;
}

/**
 * Remove the entry and files associated with an identifier.
 *
 * @param state The store state to use.
 * @param bse The entry to invalidate.
 * @return NSERROR_OK on sucess or error code on failure.
 */
static nserror
invalidate_entry(struct store_state *state, struct store_entry *bse)
{
	nserror ret;

	/* mark entry as invalid */
	bse->flags |= ENTRY_FLAGS_INVALID;

	/* check if the entry has storage already allocated */
	if (((bse->elem[ENTRY_ELEM_DATA].flags &
	     (ENTRY_ELEM_FLAG_HEAP | ENTRY_ELEM_FLAG_MMAP)) != 0) ||
	    ((bse->elem[ENTRY_ELEM_META].flags &
	      (ENTRY_ELEM_FLAG_HEAP | ENTRY_ELEM_FLAG_MMAP)) != 0)) {
		/*
		 * This entry cannot be immediately removed as it has
		 * associated allocation so wait for allocation release.
		 */
		LOG("invalidating entry with referenced allocation");
		return NSERROR_OK;
	}

	LOG("Removing entry for %p", bse);

	/* remove the entry from the index */
	ret = remove_store_entry(state, &bse);
	if (ret != NSERROR_OK) {
		return ret;
	}

	ret = invalidate_element(state, bse, ENTRY_ELEM_META);
	if (ret != NSERROR_OK) {
		LOG("Error invalidating metadata element");
	}

	ret = invalidate_element(state, bse, ENTRY_ELEM_DATA);
	if (ret != NSERROR_OK) {
		LOG("Error invalidating data element");
	}

	return NSERROR_OK;
}


/**
 * Quick sort comparison.
 */
static int compar(const void *va, const void *vb)
{
	const struct store_entry *a = &BS_ENTRY(*(entry_ident_t *)va, storestate);
	const struct store_entry *b = &BS_ENTRY(*(entry_ident_t *)vb, storestate);

	/* consider the allocation flags - if an entry has an
	 * allocation it is considered more valuble as it cannot be
	 * freed.
	 */
	if ((a->elem[ENTRY_ELEM_DATA].flags == ENTRY_ELEM_FLAG_NONE) &&
	    (b->elem[ENTRY_ELEM_DATA].flags != ENTRY_ELEM_FLAG_NONE)) {
		return -1;
	} else if ((a->elem[ENTRY_ELEM_DATA].flags != ENTRY_ELEM_FLAG_NONE) &&
		   (b->elem[ENTRY_ELEM_DATA].flags == ENTRY_ELEM_FLAG_NONE)) {
		return 1;
	}

	if ((a->elem[ENTRY_ELEM_META].flags == ENTRY_ELEM_FLAG_NONE) &&
	    (b->elem[ENTRY_ELEM_META].flags != ENTRY_ELEM_FLAG_NONE)) {
		return -1;
	} else if ((a->elem[ENTRY_ELEM_META].flags != ENTRY_ELEM_FLAG_NONE) &&
		   (b->elem[ENTRY_ELEM_META].flags == ENTRY_ELEM_FLAG_NONE)) {
		return 1;
	}

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

	LOG("Evicting entries to reduce %"PRIu64" by %zd",
	    state->total_alloc, state->hysteresis);

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
		struct store_entry *bse;

		bse = &BS_ENTRY(elist[ent], state);

		removed += bse->elem[ENTRY_ELEM_DATA].size;
		removed += bse->elem[ENTRY_ELEM_META].size;

		ret = invalidate_entry(state, bse);
		if (ret != NSERROR_OK) {
			break;
		}

		if (removed > state->hysteresis) {
			break;
		}
	}

	free(elist);

	LOG("removed %zd in %d entries", removed, ent);

	return ret;
}


/**
 * Write filesystem entries to file.
 *
 * Serialise entry index out to storage.
 *
 * @param state The backing store state to serialise.
 * @return NSERROR_OK on sucess or error code on faliure.
 */
static nserror write_entries(struct store_state *state)
{
	int fd;
	char *tname = NULL; /* temporary file name for atomic replace */
	char *fname = NULL; /* target filename */
	size_t entries_size;
	size_t written;
	nserror ret;

	if (state->entries_dirty == false) {
		/* entries have not been updated since last write */
		return NSERROR_OK;
	}

	ret = netsurf_mkpath(&tname, NULL, 2, state->path, "t"ENTRIES_FNAME);
	if (ret != NSERROR_OK) {
		return ret;
	}

	fd = open(tname, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		free(tname);
		return NSERROR_SAVE_FAILED;
	}

	entries_size = state->last_entry * sizeof(struct store_entry);

	written = (size_t)write(fd, state->entries, entries_size);

	close(fd);

	/* check all data was written */
	if (written != entries_size) {
		unlink(tname);
		free(tname);
		return NSERROR_SAVE_FAILED;
	}

	ret = netsurf_mkpath(&fname, NULL, 2, state->path, ENTRIES_FNAME);
	if (ret != NSERROR_OK) {
		unlink(tname);
		free(tname);
		return ret;
	}

	/* remove() call is to handle non-POSIX rename() implementations */
	(void)remove(fname);
	if (rename(tname, fname) != 0) {
		unlink(tname);
		free(tname);
		free(fname);
		return NSERROR_SAVE_FAILED;
	}

	return NSERROR_OK;
}

/**
 * Write block file use map to file.
 *
 * Serialise block file use map out to storage.
 *
 * \param state The backing store state to serialise.
 * \return NSERROR_OK on sucess or error code on faliure.
 */
static nserror write_blocks(struct store_state *state)
{
	int fd;
	char *tname = NULL; /* temporary file name for atomic replace */
	char *fname = NULL; /* target filename */
	size_t blocks_size;
	size_t written = 0;
	size_t wr;
	nserror ret;
	int bfidx; /* block file index */
	int elem_idx;

	if (state->blocks_dirty == false) {
		/* blocks use maps have not been updated since last write */
		return NSERROR_OK;
	}

	ret = netsurf_mkpath(&tname, NULL, 2, state->path, "t"BLOCKS_FNAME);
	if (ret != NSERROR_OK) {
		return ret;
	}

	fd = open(tname, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		free(tname);
		return NSERROR_SAVE_FAILED;
	}

	blocks_size = (BLOCK_FILE_COUNT * ENTRY_ELEM_COUNT) * BLOCK_USE_MAP_SIZE;

	for (elem_idx = 0; elem_idx < ENTRY_ELEM_COUNT; elem_idx++) {
		for (bfidx = 0; bfidx < BLOCK_FILE_COUNT; bfidx++) {
			wr = write(fd,
				   &state->blocks[elem_idx][bfidx].use_map[0],
				   BLOCK_USE_MAP_SIZE);
			if (wr != BLOCK_USE_MAP_SIZE) {
				LOG("writing block file %d use index on file number %d failed", elem_idx, bfidx);
				goto wr_err;
			}
			written += wr;
		}
	}
wr_err:
	close(fd);

	/* check all data was written */
	if (written != blocks_size) {
		unlink(tname);
		free(tname);
		return NSERROR_SAVE_FAILED;
	}

	ret = netsurf_mkpath(&fname, NULL, 2, state->path, BLOCKS_FNAME);
	if (ret != NSERROR_OK) {
		unlink(tname);
		free(tname);
		return ret;
	}

	/* remove() call is to handle non-POSIX rename() implementations */
	(void)remove(fname);
	if (rename(tname, fname) != 0) {
		unlink(tname);
		free(tname);
		free(fname);
		return NSERROR_SAVE_FAILED;
	}

	return NSERROR_OK;
}

/**
 * Ensures block files are of the correct extent
 *
 * block files have their extent set to their maximum size to ensure
 * subsequent reads and writes do not need to extend teh file and are
 * therefore faster.
 *
 * \param state The backing store state to set block extent for.
 * \return NSERROR_OK on sucess or error code on faliure.
 */
static nserror set_block_extents(struct store_state *state)
{
	int bfidx; /* block file index */
	int elem_idx;
	int ftr;

	if (state->blocks_opened == false) {
		/* no blocks have been opened since last write */
		return NSERROR_OK;
	}

	LOG("Starting");
	for (elem_idx = 0; elem_idx < ENTRY_ELEM_COUNT; elem_idx++) {
		for (bfidx = 0; bfidx < BLOCK_FILE_COUNT; bfidx++) {
			if (state->blocks[elem_idx][bfidx].fd != -1) {
				/* ensure block file is correct extent */
				ftr = ftruncate(state->blocks[elem_idx][bfidx].fd, 1U << (log2_block_size[elem_idx] + BLOCK_ENTRY_COUNT));
				if (ftr == -1) {
					LOG("Truncate failed errno:%d", errno);
				}
			}
		}
	}
	LOG("Complete");

	state->blocks_opened = false;

	return NSERROR_OK;
}

/**
 * maintinance of control structures.
 *
 * callback scheduled when control data has been update. Currently
 * this is for when the entries table is dirty and requires
 * serialising.
 *
 * \param s store state to maintain.
 */
static void control_maintinance(void *s)
{
	struct store_state *state = s;

	write_entries(state);
	write_blocks(state);
	set_block_extents(state);
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

	LOG("url:%s", nsurl_access(url));

	/* use the url hash as the entry identifier */
	ident = nsurl_hash(url);

	sei = BS_ENTRY_INDEX(ident, state);

	if (sei == 0) {
		LOG("Failed to find ident 0x%x in index", ident);
		return NSERROR_NOT_FOUND;
	}

	if (state->entries[sei].ident != ident) {
		/* entry ident did not match */
		LOG("ident did not match entry");
		return NSERROR_NOT_FOUND;
	}

	*bse = &state->entries[sei];

	state->entries[sei].last_used = time(NULL);
	state->entries[sei].use_count++;

	state->entries_dirty = true;

	guit->misc->schedule(CONTROL_MAINT_TIME, control_maintinance, state);

	return NSERROR_OK;
}

/**
 * Find next available small block.
 */
static block_index_t alloc_block(struct store_state *state, int elem_idx)
{
	int bf;
	int idx;
	int bit;
	uint8_t *map;

	for (bf = 0; bf < BLOCK_FILE_COUNT; bf++) {
		map = &state->blocks[elem_idx][bf].use_map[0];

		for (idx = 0; idx < BLOCK_USE_MAP_SIZE; idx++) {
			if (*(map + idx) != 0xff) {
				/* located an unused block */
				for (bit = 0; bit < 8;bit++) {
					if (((*(map + idx)) & (1U << bit)) == 0) {
						/* mark block as used */
						*(map + idx) |= 1U << bit;
						state->blocks_dirty = true;
						return (((bf * BLOCK_USE_MAP_SIZE) + idx) * 8) + bit;
					}
				}
			}
		}
	}

	return 0;
}

/**
 * Set a backing store entry in the entry table from a url.
 *
 * This creates a backing store entry in the entry table for a url.
 *
 * @param state The store state to use.
 * @param url The value used as the unique key to search entries for.
 * @param elem_idx The index of the entry element to use.
 * @param data The data to store
 * @param datalen The length of data in \a data
 * @param bse Pointer used to return value.
 * @return NSERROR_OK and \a bse updated on success or NSERROR_NOT_FOUND
 *         if no entry coresponds to the url.
 */
static nserror
set_store_entry(struct store_state *state,
		nsurl *url,
		int elem_idx,
		uint8_t *data,
		const size_t datalen,
		struct store_entry **bse)
{
	entry_ident_t ident;
	entry_index_t sei; /* store entry index */
	struct store_entry *se;
	nserror ret;
	struct store_entry_element *elem;

	LOG("url:%s", nsurl_access(url));

	/* evict entries as required and ensure there is at least one
	 * new entry available.
	 */
	ret = store_evict(state);
	if (ret != NSERROR_OK) {
		return ret;
	}

	/* use the url hash as the entry identifier */
	ident = nsurl_hash(url);

	/* get the entry index from the ident */
	sei = BS_ENTRY_INDEX(ident, state);
	if (sei == 0) {
		/* allocating the next available entry */
		sei = state->last_entry;
		state->last_entry++;
		BS_ENTRY_INDEX(ident, state) = sei;

		/* the entry */
		se = &state->entries[sei];

		/* clear the new entry */
		memset(se, 0, sizeof(struct store_entry));
	} else {
		/* index found existing entry */

		/* the entry */
		se = &state->entries[sei];

		if (se->ident != ident) {
			/** @todo Is there a better heuristic than
			 * first come, first served? Should we check
			 * to see if the old entry is in use and if
			 * not prefer the newly stored entry instead?
			 */
			LOG("Entry index collision trying to replace %x with %x", se->ident, ident);
			return NSERROR_PERMISSION;
		}
	}

	/* the entry element */
	elem = &se->elem[elem_idx];

	/* check if the element has storage already allocated */
	if ((elem->flags & (ENTRY_ELEM_FLAG_HEAP | ENTRY_ELEM_FLAG_MMAP)) != 0) {
		/* this entry cannot be removed as it has associated
		 * allocation.
		 */
		LOG("attempt to overwrite entry with in use data");
		return NSERROR_PERMISSION;
	}

	/* set the common entry data */
	se->ident = ident;
	se->use_count = 1;
	se->last_used = time(NULL);

	/* store the data in the element */
	elem->flags |= ENTRY_ELEM_FLAG_HEAP;
	elem->data = data;
	elem->ref = 1;

	/* account for size of entry element */
	state->total_alloc -= elem->size;
	elem->size = datalen;
	state->total_alloc += elem->size;

	/* if the elemnt will fit in a small block attempt to allocate one */
	if (elem->size <= (1U << log2_block_size[elem_idx])) {
		elem->block = alloc_block(state, elem_idx);
	}

	/* ensure control maintinance scheduled. */
	state->entries_dirty = true;
	guit->misc->schedule(CONTROL_MAINT_TIME, control_maintinance, state);

	*bse = se;

	return NSERROR_OK;
}


/**
 * Open a file using a store ident.
 *
 * @param state The store state to use.
 * @param ident The identifier to open file for.
 * @param elem_idx The element within the store entry to open. The
 *                 value should be be one of the values in the
 *                 store_entry_elem_idx enum. Additionally it may have
 *                 ENTRY_ELEM_COUNT added to it to indicate block file
 *                 names.
 * @param openflags The flags used with the open call.
 * @return An fd from the open call or -1 on error.
 */
static int
store_open(struct store_state *state,
	   entry_ident_t ident,
	   int elem_idx,
	   int openflags)
{
	char *fname;
	nserror ret;
	int fd;

	fname = store_fname(state, ident, elem_idx);
	if (fname == NULL) {
		LOG("filename error");
		return -1;
	}

	/* ensure all path elements to file exist if creating file */
	if (openflags & O_CREAT) {
		ret = netsurf_mkdir_all(fname);
		if (ret != NSERROR_OK) {
			LOG("file path \"%s\" could not be created", fname);
			free(fname);
			return -1;
		}
	}

	LOG("opening %s", fname);
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

	LOG("Allocating %ld bytes for max of %d buckets",
	    (1 << state->ident_bits) * sizeof(entry_index_t),
	    1 << state->ident_bits);

	state->addrmap = calloc(1 << state->ident_bits, sizeof(entry_index_t));
	if (state->addrmap == NULL) {
		return NSERROR_NOMEM;
	}

	state->total_alloc = 0;

	for (eloop = 1; eloop < state->last_entry; eloop++) {
		/*
		LOG("entry:%d ident:0x%08x used:%d",
		     eloop,
		     BS_ADDRESS(state->entries[eloop].ident, state),
		     state->entries[eloop].use_count);
		*/

		/* update the address map to point at the entry */
		BS_ENTRY_INDEX(state->entries[eloop].ident, state) = eloop;

		/* account for the storage space */
		state->total_alloc += state->entries[eloop].elem[ENTRY_ELEM_DATA].size;
		state->total_alloc += state->entries[eloop].elem[ENTRY_ELEM_META].size;
		/* ensure entry does not have any allocation state */
		state->entries[eloop].elem[ENTRY_ELEM_DATA].flags &= ~(ENTRY_ELEM_FLAG_HEAP | ENTRY_ELEM_FLAG_MMAP);
		state->entries[eloop].elem[ENTRY_ELEM_META].flags &= ~(ENTRY_ELEM_FLAG_HEAP | ENTRY_ELEM_FLAG_MMAP);
	}

	return NSERROR_OK;
}

/**
 * Unlink entries file
 *
 * @param state The backing store state.
 * @return NSERROR_OK on sucess or error code on faliure.
 */
static nserror
unlink_entries(struct store_state *state)
{
	char *fname = NULL;
	nserror ret;

	ret = netsurf_mkpath(&fname, NULL, 2, state->path, ENTRIES_FNAME);
	if (ret != NSERROR_OK) {
		return ret;
	}

	unlink(fname);

	free(fname);
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

	ret = netsurf_mkpath(&fname, NULL, 2, state->path, ENTRIES_FNAME);
	if (ret != NSERROR_OK) {
		return ret;
	}

	entries_size = (1 << state->entry_bits) * sizeof(struct store_entry);

	LOG("Allocating %zd bytes for max of %d entries of %ld length elements %ld length",
	    entries_size, 1 << state->entry_bits,
	    sizeof(struct store_entry),
	    sizeof(struct store_entry_element));

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
			LOG("Read %d entries", state->last_entry);
		}
	} else {
		/* could rebuild entries from fs */
		state->last_entry = 1;
	}
	return NSERROR_OK;
}


/**
 * Read block file usage bitmaps.
 *
 * @param state The backing store state to put the loaded entries in.
 * @return NSERROR_OK on sucess or error code on faliure.
 */
static nserror
read_blocks(struct store_state *state)
{
	int bfidx; /* block file index */
	int elem_idx;
	int fd;
	ssize_t rd;
	char *fname = NULL;
	nserror ret;

	ret = netsurf_mkpath(&fname, NULL, 2, state->path, BLOCKS_FNAME);
	if (ret != NSERROR_OK) {
		return ret;
	}

	LOG("Initialising block use map from %s", fname);

	fd = open(fname, O_RDWR);
	free(fname);
	if (fd != -1) {
		/* initialise block file use array */
		for (elem_idx = 0; elem_idx < ENTRY_ELEM_COUNT; elem_idx++) {
			for (bfidx = 0; bfidx < BLOCK_FILE_COUNT; bfidx++) {
				rd = read(fd,
					  &state->blocks[elem_idx][bfidx].use_map[0],
					  BLOCK_USE_MAP_SIZE);
				if (rd <= 0) {
					LOG("reading block file %d use index on file number %d failed", elem_idx, bfidx);
					goto rd_err;
				}
			}
		}
	rd_err:
		close(fd);

	} else {
		LOG("Initialising block use map to defaults");
		/* ensure block 0 (invalid sentinal) is skipped */
		state->blocks[ENTRY_ELEM_DATA][0].use_map[0] = 1;
		state->blocks[ENTRY_ELEM_META][0].use_map[0] = 1;
	}

	/* initialise block file file descriptors */
	for (bfidx = 0; bfidx < BLOCK_FILE_COUNT; bfidx++) {
		state->blocks[ENTRY_ELEM_DATA][bfidx].fd = -1;
		state->blocks[ENTRY_ELEM_META][bfidx].fd = -1;
	}

	return NSERROR_OK;
}

/**
 * Write the cache tag file.
 *
 * @param state The cache state.
 * @return NSERROR_OK on sucess or error code on faliure.
 */
static nserror
write_cache_tag(struct store_state *state)
{
	FILE *fcachetag;
	nserror ret;
	char *fname = NULL;

	ret = netsurf_mkpath(&fname, NULL, 2, state->path, "CACHEDIR.TAG");
	if (ret != NSERROR_OK) {
		return ret;
	}

	fcachetag = fopen(fname, "wb");

	free(fname);

	if (fcachetag == NULL) {
		return NSERROR_NOT_FOUND;
	}

	fprintf(fcachetag,
		"Signature: 8a477f597d28d172789f06886806bc55\n"
		"# This file is a cache directory tag created by NetSurf.\n"
		"# For information about cache directory tags, see:\n"
		"#	http://www.brynosaurus.com/cachedir/\n");

	fclose(fcachetag);

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

	LOG("writing control file \"%s\"", fname);

	ret = netsurf_mkdir_all(fname);
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

	LOG("opening control file \"%s\"", fname);

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
		goto control_error;
	}
	if (fgetc(fcontrol) != 0) {
		goto control_error;
	}

	/* second line is log2 size of address hash */
	if (fscanf(fcontrol, "%u", &addrbits) != 1) {
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

	/* read store control and create new if required */
	ret = read_control(newstate);
	if (ret != NSERROR_OK) {
		LOG("read control failed %s", messages_get_errorcode(ret));
		ret = write_control(newstate);
		if (ret == NSERROR_OK) {
			unlink_entries(newstate);
			write_cache_tag(newstate);
		}
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
		/* that obviously went well */
		free(newstate->path);
		free(newstate);
		return ret;
	}

	ret = read_blocks(newstate);
	if (ret != NSERROR_OK) {
		/* oh dear */
		free(newstate->path);
		free(newstate);
		return ret;
	}

	storestate = newstate;

	LOG("FS backing store init successful");

	LOG("path:%s limit:%zd hyst:%zd addr:%d entries:%d",
	    newstate->path,
	    newstate->limit,
	    newstate->hysteresis,
	    newstate->ident_bits,
	    newstate->entry_bits);
	LOG("Using %"PRIu64"/%zd", newstate->total_alloc, newstate->limit);

	return NSERROR_OK;
}


/**
 * Finalise the backing store.
 *
 * \todo This will cause the backing store to leak any outstanding memory
 * allocations. This will probably best be done by a global use count.
 *
 * @return NSERROR_OK on success.
 */
static nserror
finalise(void)
{
	int bf; /* block file index */
	unsigned int op_count;

	if (storestate != NULL) {
		guit->misc->schedule(-1, control_maintinance, storestate);
		write_entries(storestate);
		write_blocks(storestate);

		/* ensure all block files are closed */
		for (bf = 0; bf < BLOCK_FILE_COUNT; bf++) {
			if (storestate->blocks[ENTRY_ELEM_DATA][bf].fd != -1) {
				close(storestate->blocks[ENTRY_ELEM_DATA][bf].fd);
			}
			if (storestate->blocks[ENTRY_ELEM_META][bf].fd != -1) {
				close(storestate->blocks[ENTRY_ELEM_META][bf].fd);
			}
		}

		op_count = storestate->hit_count + storestate->miss_count;

		/* avoid division by zero */
		if (op_count > 0) {
			LOG("Cache total/hit/miss/fail (counts) %d/%zd/%zd/%d (100%%/%zd%%/%zd%%/%d%%)",
			    op_count,
			    storestate->hit_count,
			    storestate->miss_count,
			    0,
			    (storestate->hit_count * 100) / op_count,
			    (storestate->miss_count * 100) / op_count,
			    0);
		}

		free(storestate->path);
		free(storestate);
		storestate = NULL;
	}
	return NSERROR_OK;
}


/**
 * Write an element of an entry to backing storage in a small block file.
 *
 * \param state The backing store state to use.
 * \param bse The entry to store
 * \param elem_idx The element index within the entry.
 * \return NSERROR_OK on success or error code.
 */
static nserror store_write_block(struct store_state *state,
			 struct store_entry *bse,
			 int elem_idx)
{
	block_index_t bf = (bse->elem[elem_idx].block >> BLOCK_ENTRY_COUNT) &
		((1 << BLOCK_FILE_COUNT) - 1); /* block file block resides in */
	block_index_t bi = bse->elem[elem_idx].block & ((1U << BLOCK_ENTRY_COUNT) -1); /* block index in file */
	ssize_t wr;
	off_t offst;

	/* ensure the block file fd is good */
	if (state->blocks[elem_idx][bf].fd == -1) {
		state->blocks[elem_idx][bf].fd = store_open(state, bf,
				elem_idx + ENTRY_ELEM_COUNT, O_CREAT | O_RDWR);
		if (state->blocks[elem_idx][bf].fd == -1) {
			LOG("Open failed errno %d", errno);
			return NSERROR_SAVE_FAILED;
		}

		/* flag that a block file has been opened */
		state->blocks_opened = true;
	}

	offst = bi << log2_block_size[elem_idx];

	wr = nsu_pwrite(state->blocks[elem_idx][bf].fd,
		    bse->elem[elem_idx].data,
		    bse->elem[elem_idx].size,
		    offst);
	if (wr != (ssize_t)bse->elem[elem_idx].size) {
		LOG("Write failed %zd of %d bytes from %p at 0x%jx block %d errno %d",
		    wr,
		    bse->elem[elem_idx].size,
		    bse->elem[elem_idx].data,
		    (uintmax_t)offst,
		    bse->elem[elem_idx].block,
		    errno);
		return NSERROR_SAVE_FAILED;
	}

	LOG("Wrote %zd bytes from %p at 0x%jx block %d",
	    wr,
	    bse->elem[elem_idx].data,
	    (uintmax_t)offst,
	    bse->elem[elem_idx].block);

	return NSERROR_OK;
}

/**
 * Write an element of an entry to backing storage as an individual file.
 *
 * \param state The backing store state to use.
 * \param bse The entry to store
 * \param elem_idx The element index within the entry.
 * \return NSERROR_OK on success or error code.
 */
static nserror store_write_file(struct store_state *state,
			 struct store_entry *bse,
			 int elem_idx)
{
	ssize_t wr;
	int fd;
	int err;

	fd = store_open(state, bse->ident, elem_idx, O_CREAT | O_WRONLY);
	if (fd < 0) {
		perror("");
		LOG("Open failed %d errno %d", fd, errno);
		return NSERROR_SAVE_FAILED;
	}

	wr = write(fd, bse->elem[elem_idx].data, bse->elem[elem_idx].size);
	err = errno; /* close can change errno */

	close(fd);
	if (wr != (ssize_t)bse->elem[elem_idx].size) {
		LOG("Write failed %zd of %d bytes from %p errno %d",
		    wr,
		    bse->elem[elem_idx].size,
		    bse->elem[elem_idx].data,
		    err);

		/** @todo Delete the file? */
		return NSERROR_SAVE_FAILED;
	}

	LOG("Wrote %zd bytes from %p", wr, bse->elem[elem_idx].data);

	return NSERROR_OK;
}

/**
 * Place an object in the backing store.
 *
 * takes ownership of the heap block passed in.
 *
 * @param url The url is used as the unique primary key for the data.
 * @param bsflags The flags to control how the object is stored.
 * @param data The objects source data.
 * @param datalen The length of the \a data.
 * @return NSERROR_OK on success or error code on faliure.
 */
static nserror
store(nsurl *url,
      enum backing_store_flags bsflags,
      uint8_t *data,
      const size_t datalen)
{
	nserror ret;
	struct store_entry *bse;
	int elem_idx;

	/* check backing store is initialised */
	if (storestate == NULL) {
		return NSERROR_INIT_FAILED;
	}

	/* calculate the entry element index */
	if ((bsflags & BACKING_STORE_META) != 0) {
		elem_idx = ENTRY_ELEM_META;
	} else {
		elem_idx = ENTRY_ELEM_DATA;
	}

	/* set the store entry up */
	ret = set_store_entry(storestate, url, elem_idx, data, datalen, &bse);
	if (ret != NSERROR_OK) {
		LOG("store entry setting failed");
		return ret;
	}

	if (bse->elem[elem_idx].block != 0) {
		/* small block storage */
		ret = store_write_block(storestate, bse, elem_idx);
	} else {
		/* separate file in backing store */
		ret = store_write_file(storestate, bse, elem_idx);
	}

	return ret;
}

/**
 * release any allocation for an entry
 */
static nserror entry_release_alloc(struct store_entry_element *elem)
{
	if ((elem->flags & ENTRY_ELEM_FLAG_HEAP) != 0) {
		elem->ref--;
		if (elem->ref == 0) {
			LOG("freeing %p", elem->data);
			free(elem->data);
			elem->flags &= ~ENTRY_ELEM_FLAG_HEAP;
		}
	}
	return NSERROR_OK;
}


/**
 * Read an element of an entry from a small block file in the backing storage.
 *
 * \param state The backing store state to use.
 * \param bse The entry to read.
 * \param elem_idx The element index within the entry.
 * \return NSERROR_OK on success or error code.
 */
static nserror store_read_block(struct store_state *state,
			 struct store_entry *bse,
			 int elem_idx)
{
	block_index_t bf = (bse->elem[elem_idx].block >> BLOCK_ENTRY_COUNT) &
		((1 << BLOCK_FILE_COUNT) - 1); /* block file block resides in */
	block_index_t bi = bse->elem[elem_idx].block & ((1 << BLOCK_ENTRY_COUNT) -1); /* block index in file */
	ssize_t rd;
	off_t offst;

	/* ensure the block file fd is good */
	if (state->blocks[elem_idx][bf].fd == -1) {
		state->blocks[elem_idx][bf].fd = store_open(state, bf,
				elem_idx + ENTRY_ELEM_COUNT, O_CREAT | O_RDWR);
		if (state->blocks[elem_idx][bf].fd == -1) {
			LOG("Open failed errno %d", errno);
			return NSERROR_SAVE_FAILED;
		}

		/* flag that a block file has been opened */
		state->blocks_opened = true;
	}

	offst = bi << log2_block_size[elem_idx];

	rd = nsu_pread(state->blocks[elem_idx][bf].fd,
		   bse->elem[elem_idx].data,
		   bse->elem[elem_idx].size,
		   offst);
	if (rd != (ssize_t)bse->elem[elem_idx].size) {
		LOG("Failed reading %zd of %d bytes into %p from 0x%jx block %d errno %d",
		    rd,
		    bse->elem[elem_idx].size,
		    bse->elem[elem_idx].data,
		    (uintmax_t)offst,
		    bse->elem[elem_idx].block,
		    errno);
		return NSERROR_SAVE_FAILED;
	}

	LOG("Read %zd bytes into %p from 0x%jx block %d",
	    rd,
	    bse->elem[elem_idx].data,
	    (uintmax_t)offst,
	    bse->elem[elem_idx].block);

	return NSERROR_OK;
}

/**
 * Read an element of an entry from an individual file in the backing storage.
 *
 * \param state The backing store state to use.
 * \param bse The entry to read.
 * \param elem_idx The element index within the entry.
 * \return NSERROR_OK on success or error code.
 */
static nserror store_read_file(struct store_state *state,
			 struct store_entry *bse,
			 int elem_idx)
{
	int fd;
	ssize_t rd; /* return from read */
	int ret = NSERROR_OK;
	size_t tot = 0; /* total size */

	/* separate file in backing store */
	fd = store_open(storestate, bse->ident, elem_idx, O_RDONLY);
	if (fd < 0) {
		LOG("Open failed %d errno %d", fd, errno);
		/** @todo should this invalidate the entry? */
		return NSERROR_NOT_FOUND;
	}

	while (tot < bse->elem[elem_idx].size) {
		rd = read(fd,
			  bse->elem[elem_idx].data + tot,
			  bse->elem[elem_idx].size - tot);
		if (rd <= 0) {
			LOG("read error returned %zd errno %d", rd, errno);
			ret = NSERROR_NOT_FOUND;
			break;
		}
		tot += rd;
	}

	close(fd);

	LOG("Read %zd bytes into %p", tot, bse->elem[elem_idx].data);

	return ret;
}

/**
 * Retrive an object from the backing store.
 *
 * @param[in] url The url is used as the unique primary key for the data.
 * @param[in] bsflags The flags to control how the object is retrieved.
 * @param[out] data_out The objects data.
 * @param[out] datalen_out The length of the \a data retrieved.
 * @return NSERROR_OK on success or error code on faliure.
 */
static nserror
fetch(nsurl *url,
      enum backing_store_flags bsflags,
      uint8_t **data_out,
      size_t *datalen_out)
{
	nserror ret;
	struct store_entry *bse;
	struct store_entry_element *elem;
	int elem_idx;

	/* check backing store is initialised */
	if (storestate == NULL) {
		return NSERROR_INIT_FAILED;
	}

	/* fetch store entry */
	ret = get_store_entry(storestate, url, &bse);
	if (ret != NSERROR_OK) {
		LOG("entry not found");
		storestate->miss_count++;
		return ret;
	}
	storestate->hit_count++;

	LOG("retriving cache data for url:%s", nsurl_access(url));

	/* calculate the entry element index */
	if ((bsflags & BACKING_STORE_META) != 0) {
		elem_idx = ENTRY_ELEM_META;
	} else {
		elem_idx = ENTRY_ELEM_DATA;
	}
	elem = &bse->elem[elem_idx];

	/* if an allocation already exists return it */
	if ((elem->flags & ENTRY_ELEM_FLAG_HEAP) != 0) {
		/* use the existing allocation and bump the ref count. */
		elem->ref++;

		LOG("Using existing entry (%p) allocation %p refs:%d", bse, elem->data, elem->ref);

	} else {
		/* allocate from the heap */
		elem->data = malloc(elem->size);
		if (elem->data == NULL) {
			LOG("Failed to create new heap allocation");
			return NSERROR_NOMEM;
		}
		LOG("Created new heap allocation %p", elem->data);

		/* mark the entry as having a valid heap allocation */
		elem->flags |= ENTRY_ELEM_FLAG_HEAP;
		elem->ref = 1;

		/* fill the new block */
		if (elem->block != 0) {
			ret = store_read_block(storestate, bse, elem_idx);
		} else {
			ret = store_read_file(storestate, bse, elem_idx);
		}
	}

	/* free the allocation if there is a read error */
	if (ret != NSERROR_OK) {
		entry_release_alloc(elem);
	} else {
		/* update stats and setup return pointers */
		storestate->hit_size += elem->size;

		*data_out = elem->data;
		*datalen_out = elem->size;
	}

	return ret;
}


/**
 * release a previously fetched or stored memory object.
 *
 * @param[in] url The url is used as the unique primary key to invalidate.
 * @param[in] bsflags The flags to control how the object data is released.
 * @return NSERROR_OK on success or error code on faliure.
 */
static nserror release(nsurl *url, enum backing_store_flags bsflags)
{
	nserror ret;
	struct store_entry *bse;
	struct store_entry_element *elem;

	/* check backing store is initialised */
	if (storestate == NULL) {
		return NSERROR_INIT_FAILED;
	}

	ret = get_store_entry(storestate, url, &bse);
	if (ret != NSERROR_OK) {
		LOG("entry not found");
		return ret;
	}

	/* the entry element */
	if ((bsflags & BACKING_STORE_META) != 0) {
		elem = &bse->elem[ENTRY_ELEM_META];
	} else {
		elem = &bse->elem[ENTRY_ELEM_DATA];
	}

	ret = entry_release_alloc(elem);

	/* if the entry has previously been invalidated but had
	 * allocation it must be invalidated fully now the allocation
	 * has been released.
	 */
	if ((ret == NSERROR_OK) &&
	    ((bse->flags & ENTRY_FLAGS_INVALID) != 0)) {
		ret = invalidate_entry(storestate, bse);
	}

	return ret;
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
	nserror ret;
	struct store_entry *bse;

	/* check backing store is initialised */
	if (storestate == NULL) {
		return NSERROR_INIT_FAILED;
	}

	ret = get_store_entry(storestate, url, &bse);
	if (ret != NSERROR_OK) {
		return ret;
	}

	return invalidate_entry(storestate, bse);
}


static struct gui_llcache_table llcache_table = {
	.initialise = initialise,
	.finalise = finalise,
	.store = store,
	.fetch = fetch,
	.invalidate = invalidate,
	.release = release,
};

struct gui_llcache_table *filesystem_llcache_table = &llcache_table;
