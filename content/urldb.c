/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2006 John M Bell <jmb202@ecs.soton.ac.uk>
 */

/** \file
 * Unified URL information database (implementation)
 *
 * URLs are stored in a tree-based structure as follows:
 *
 * The host component is extracted from each URL and, if a FQDN, split on
 * every '.'.The tree is constructed by inserting each FQDN segment in
 * reverse order. Duplicate nodes are merged.
 *
 * If the host part of an URL is an IP address, then this is added to the
 * tree verbatim (as if it were a TLD).
 *
 * This provides something looking like:
 *
 * 			      root (a sentinel)
 * 				|
 * 	-------------------------------------------------
 * 	|	|	|	|	|	|	|
 *     com     edu     gov  127.0.0.1  net     org     uk	TLDs
 * 	|	|	|		|	|	|
 *    google   ...     ...             ...     ...     co	2LDs
 * 	|						|
 *     www					       bbc  Hosts/Subdomains
 *							|
 *						       www	...
 *
 * Each of the nodes in this tree is a struct host_part. This stores the
 * FQDN segment (or IP address) with which the node is concerned. Each node
 * may contain further information about paths on a host (struct path_data)
 * or SSL certificate processing on a host-wide basis
 * (host_part::permit_invalid_certs).
 *
 * Path data is concerned with storing various metadata about the path in
 * question. This includes global history data, HTTP authentication details
 * and any associated HTTP cookies. This is stored as a tree of path segments
 * hanging off the relevant host_part node.
 *
 * Therefore, to find the last visited time of the URL
 * http://www.example.com/path/to/resource.html, the FQDN tree would be
 * traversed in the order root -> "com" -> "example" -> "www". The "www"
 * node would have attached to it a tree of struct path_data:
 *
 *			    (sentinel)
 *				|
 * 			       path
 * 				|
 * 			       to
 * 				|
 * 			   resource.html
 *
 * This represents the absolute path "/path/to/resource.html". The leaf node
 * "resource.html" contains the last visited time of the resource.
 *
 * The mechanism described above is, however, not particularly conducive to
 * fast searching of the database for a given URL (or URLs beginning with a
 * given prefix). Therefore, an anciliary data structure is used to enable
 * fast searching. This structure simply reflects the contents of the
 * database, with entries being added/removed at the same time as for the
 * core database. In order to ensure that degenerate cases are kept to a
 * minimum, we use an AAtree. This is an approximation of a Red-Black tree
 * with similar performance characteristics, but with a significantly
 * simpler implementation. Entries in this tree comprise pointers to the
 * leaf nodes of the host tree described above.
 */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "netsurf/image/bitmap.h"
#include "netsurf/content/urldb.h"
#include "netsurf/desktop/options.h"
#ifdef riscos
/** \todo lose this */
#include "netsurf/riscos/bitmap.h"
#endif
#include "netsurf/utils/log.h"
#include "netsurf/utils/url.h"
#include "netsurf/utils/utils.h"

struct cookie {
	char *name;		/**< Cookie name */
	char *value;		/**< Cookie value */
	char *comment;		/**< Cookie comment */
	time_t expires;		/**< Expiry timestamp, or 0 for session */
	time_t last_used;	/**< Last used time */
	bool secure;		/**< Only send for HTTPS requests */
	enum { COOKIE_NETSCAPE = 0,
		COOKIE_RFC2109 = 1,
		COOKIE_RFC2965 = 2
	} version;		/**< Specification compliance */
	bool no_destroy;	/**< Never destroy this cookie,
				 * unless it's expired */

	struct cookie *next;	/**< Next in list */
};

struct auth_data {
	char *realm;		/**< Protection realm */
	char *auth;		/**< Authentication details in form
				 * username:password */
};

struct url_internal_data {
	char *title;		/**< Resource title */
	unsigned int visits;	/**< Visit count */
	time_t last_visit;	/**< Last visit time */
	content_type type;	/**< Type of resource */
};

struct path_data {
	char *url;		/**< Full URL */
	char *scheme;		/**< URL scheme for data */
	unsigned int port;	/**< Port number for data */
	char *segment;		/**< Path segment for this node */
	unsigned int frag_cnt;	/**< Number of entries in ::fragment */
	char **fragment;	/**< Array of fragments */

	struct bitmap *thumb;	/**< Thumbnail image of resource */
	struct url_internal_data urld;	/**< URL data for resource */
	struct auth_data auth;	/**< Authentication data for resource */
	struct cookie *cookies;	/**< Cookies associated with resource */

	struct path_data *next;	/**< Next sibling */
	struct path_data *prev;	/**< Previous sibling */
	struct path_data *parent;	/**< Parent path segment */
	struct path_data *children;	/**< Child path segments */
	struct path_data *last;		/**< Last child */
};

struct host_part {
	/**< Known paths on this host. This _must_ be first so that
	 * struct host_part *h = (struct host_part *)mypath; works */
	struct path_data paths;
	bool permit_invalid_certs;	/**< Allow access to SSL protected
					 * resources on this host without
					 * verifying certificate authenticity
					 */

	char *part;		/**< Part of host string */

	struct host_part *next;	/**< Next sibling */
	struct host_part *prev;	/**< Previous sibling */
	struct host_part *parent;	/**< Parent host part */
	struct host_part *children;	/**< Child host parts */
};

struct search_node {
	const struct host_part *data;	/**< Host tree entry */

	unsigned int level;		/**< Node level */

	struct search_node *left;	/**< Left subtree */
	struct search_node *right;	/**< Right subtree */
};

/* Saving */
static void urldb_save_search_tree(struct search_node *root, FILE *fp);
static void urldb_count_urls(const struct path_data *root, time_t expiry,
		unsigned int *count);
static void urldb_write_paths(const struct path_data *parent,
		const char *host, FILE *fp, char **path, int *path_alloc,
		int *path_used, time_t expiry);

/* Iteration */
static bool urldb_iterate_partial_host(struct search_node *root,
		const char *prefix, bool (*callback)(const char *url,
		const struct url_data *data));
static bool urldb_iterate_partial_path(const struct path_data *parent,
		const char *prefix, bool (*callback)(const char *url,
		const struct url_data *data));
static bool urldb_iterate_entries_host(struct search_node *parent,
		bool (*callback)(const char *url,
		const struct url_data *data));
static bool urldb_iterate_entries_path(const struct path_data *parent,
		bool (*callback)(const char *url,
		const struct url_data *data));

/* Insertion */
static struct host_part *urldb_add_host_node(const char *part,
		struct host_part *parent);
static struct host_part *urldb_add_host(const char *host);
static struct path_data *urldb_add_path_node(const char *scheme,
		unsigned int port, const char *segment, const char *fragment,
		struct path_data *parent);
static struct path_data *urldb_add_path(const char *scheme,
		unsigned int port, const struct host_part *host,
		const char *path, const char *fragment,
		const char *url_no_frag);
static int urldb_add_path_fragment_cmp(const void *a, const void *b);
static struct path_data *urldb_add_path_fragment(struct path_data *segment,
		const char *fragment);

/* Lookup */
static struct path_data *urldb_find_url(const char *url);
static struct path_data *urldb_match_path(const struct path_data *parent,
		const char *path, const char *scheme, unsigned short port);

/* Dump */
static void urldb_dump_hosts(struct host_part *parent);
static void urldb_dump_paths(struct path_data *parent);
static void urldb_dump_search(struct search_node *parent, int depth);

/* Search tree */
static struct search_node *urldb_search_insert(struct search_node *root,
		const struct host_part *data);
static struct search_node *urldb_search_insert_internal(
		struct search_node *root, struct search_node *n);
static struct search_node *urldb_search_remove(struct search_node *root,
		const struct host_part *data);
static const struct host_part *urldb_search_find(struct search_node *root,
		const char *host);
static struct search_node *urldb_search_skew(struct search_node *root);
static struct search_node *urldb_search_split(struct search_node *root);
static int urldb_search_match_host(const struct host_part *a,
		const struct host_part *b);
static int urldb_search_match_string(const struct host_part *a,
		const char *b);
static int urldb_search_match_prefix(const struct host_part *a,
		const char *b);

/** Root database handle */
static struct host_part db_root;

/** Search trees - one per letter + 1 for IPs */
#define NUM_SEARCH_TREES 27
#define ST_IP 0
#define ST_DN 1
static struct search_node empty = { 0, 0, &empty, &empty };
static struct search_node *search_trees[NUM_SEARCH_TREES] = {
	&empty, &empty, &empty, &empty, &empty, &empty, &empty, &empty,
	&empty, &empty, &empty, &empty, &empty, &empty, &empty, &empty,
	&empty, &empty, &empty, &empty, &empty, &empty, &empty, &empty,
	&empty, &empty, &empty
};

/**
 * Import an URL database from file, replacing any existing database
 *
 * \param filename Name of file containing data
 */
void urldb_load(const char *filename)
{
#define MAXIMUM_URL_LENGTH 4096
	char s[MAXIMUM_URL_LENGTH];
	char host[256];
	struct host_part *h;
	int urls;
	int i;
	int version;
	int length;
	FILE *fp;

	assert(filename);

	LOG(("Loading URL file"));

	fp = fopen(filename, "r");
	if (!fp) {
		LOG(("Failed to open file '%s' for reading", filename));
		return;
	}

	if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
		return;
	version = atoi(s);
	if (version < 105) {
		LOG(("Unsupported URL file version."));
		return;
	}
	if (version > 106) {
		LOG(("Unknown URL file version."));
		return;
	}

	while (fgets(host, sizeof host, fp)) {
		/* get the hostname */
		length = strlen(host) - 1;
		host[length] = '\0';

		/* skip data that has ended up with a host of '' */
		if (length == 0) {
			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			urls = atoi(s);
			for (i = 0; i < ((version == 105 ? 6 : 8) * urls);
					i++)
				if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
					break;
			continue;
		}

		if (version == 105) {
			/* file:/ -> localhost */
			if (strcasecmp(host, "file:/") == 0)
				snprintf(host, sizeof host, "localhost");
			else {
				/* strip any port number */
				char *colon = strrchr(host, ':');
				if (colon)
					*colon = '\0';
			}
		}

		/* read number of URLs */
		if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
			break;
		urls = atoi(s);

		/* no URLs => try next host */
		if (urls == 0) {
			LOG(("No URLs for '%s'", host));
			continue;
		}

		h = urldb_add_host(host);
		if (!h)
			die("Memory exhausted whilst loading URL file");

		/* load the non-corrupt data */
		for (i = 0; i < urls; i++) {
			struct path_data *p = NULL;

			if (version == 105) {
				if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
					break;
				length = strlen(s) - 1;
				s[length] = '\0';

				if (strncasecmp(s, "file:", 5) == 0) {
					/* local file, so fudge insertion */
					char url[7 + 4096];

					snprintf(url, sizeof url,
							"file://%s", s + 5);

					p = urldb_add_path("file", 0, h,
							s + 5, NULL, url);
					if (!p) {
						LOG(("Failed inserting '%s'",
								url));
						die("Memory exhausted "
							"whilst loading "
							"URL file");
					}
				} else {
					if (!urldb_add_url(s)) {
						LOG(("Failed inserting '%s'",
								s));
					}
					p = urldb_find_url(s);
				}
			} else {
				char scheme[64], ports[10];
				char url[64 + 3 + 256 + 6 + 4096 + 1];
				unsigned int port;
				bool is_file = false;

				if (!fgets(scheme, sizeof scheme, fp))
					break;
				length = strlen(scheme) - 1;
				scheme[length] = '\0';

				if (!fgets(ports, sizeof ports, fp))
					break;
				length = strlen(ports) - 1;
				ports[length] = '\0';
				port = atoi(ports);

				if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
					break;
				length = strlen(s) - 1;
				s[length] = '\0';

				if (!strcasecmp(host, "localhost") &&
						!strcasecmp(scheme, "file"))
					is_file = true;

				snprintf(url, sizeof url, "%s://%s%s%s%s",
						scheme,
						/* file URLs have no host */
						(is_file ? "" : host),
						(port ? ":" : ""),
						(port ? ports : ""),
						s);

				p = urldb_add_path(scheme, port, h, s, NULL,
						url);
				if (!p) {
					LOG(("Failed inserting '%s'", url));
					die("Memory exhausted whilst loading "
							"URL file");
				}
			}

			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			if (p)
				p->urld.visits = (unsigned int)atoi(s);

			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			if (p)
				p->urld.last_visit = (time_t)atoi(s);

			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			if (p)
				p->urld.type = (content_type)atoi(s);

			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
#ifdef riscos
			if (p && strlen(s) == 12) {
				/* ensure filename is 'XX.XX.XX.XX' */
				if ((s[2] == '.') && (s[5] == '.') &&
						(s[8] == '.')) {
					s[2] = '/';
					s[5] = '/';
					s[8] = '/';
					s[11] = '\0';
					p->thumb = bitmap_create_file(s);
				} else if ((s[2] == '/') && (s[5] == '/') &&
						(s[8] == '/')) {
					s[11] = '\0';
					p->thumb = bitmap_create_file(s);
				}
			}
#endif

			if (!fgets(s, MAXIMUM_URL_LENGTH, fp))
				break;
			length = strlen(s) - 1;
			if (p && length > 0) {
				s[length] = '\0';
				p->urld.title = malloc(length + 1);
				if (p->urld.title)
					memcpy(p->urld.title, s, length + 1);
			}
		}
	}

	fclose(fp);
	LOG(("Successfully loaded URL file"));
#undef MAXIMUM_URL_LENGTH
}

/**
 * Export the current database to file
 *
 * \param filename Name of file to export to
 */
void urldb_save(const char *filename)
{
	FILE *fp;
	int i;

	assert(filename);

	fp = fopen(filename, "w");
	if (!fp) {
		LOG(("Failed to open file '%s' for writing", filename));
		return;
	}

	/* file format version number */
	fprintf(fp, "106\n");

	for (i = 0; i != NUM_SEARCH_TREES; i++) {
		urldb_save_search_tree(search_trees[i], fp);
	}

	fclose(fp);
}

/**
 * Save a search (sub)tree
 *
 * \param root Root of (sub)tree to save
 * \param fp File to write to
 */
void urldb_save_search_tree(struct search_node *parent, FILE *fp)
{
	char host[256];
	const struct host_part *h;
	unsigned int path_count = 0;
	char *path, *p, *end;
	int path_alloc = 64, path_used = 2;
	time_t expiry = time(NULL) - (60 * 60 * 24) * option_expire_url;

	if (parent == &empty)
		return;

	urldb_save_search_tree(parent->left, fp);

	path = malloc(path_alloc);
	if (!path)
		return;

	path[0] = '/';
	path[1] = '\0';

	for (h = parent->data, p = host, end = host + sizeof host;
			h && h != &db_root && p < end; h = h->parent) {
		int written = snprintf(p, end - p, "%s%s", h->part,
				(h->parent && h->parent->parent) ? "." : "");
		if (written < 0) {
			free(path);
			return;
		}
		p += written;
	}

	urldb_count_urls(&parent->data->paths, expiry, &path_count);

	if (path_count > 0) {
		fprintf(fp, "%s\n%i\n", host, path_count);

		urldb_write_paths(&parent->data->paths, host, fp,
				&path, &path_alloc, &path_used, expiry);
	}

	free(path);

	urldb_save_search_tree(parent->right, fp);
}

/**
 * Count number of URLs associated with a host
 *
 * \param root Root of path data tree
 * \param expiry Expiry time for URLs
 * \param count Pointer to count
 */
void urldb_count_urls(const struct path_data *root, time_t expiry,
		unsigned int *count)
{
	const struct path_data *p;

	if (!root->children) {
		if ((root->urld.last_visit > expiry) &&
				(root->urld.visits > 0))
			(*count)++;
	}

	for (p = root->children; p; p = p->next)
		urldb_count_urls(p, expiry, count);
}

/**
 * Write paths associated with a host
 *
 * \param parent Root of (sub)tree to write
 * \param host Current host name
 * \param fp File to write to
 * \param path Current path string
 * \param path_alloc Allocated size of path
 * \param path_used Used size of path
 * \param expiry Expiry time of URLs
 */
void urldb_write_paths(const struct path_data *parent, const char *host,
		FILE *fp, char **path, int *path_alloc, int *path_used,
		time_t expiry)
{
	const struct path_data *p;
	int i;
	int pused = *path_used;

	if (!parent->children) {
		/* leaf node */
		if (!((parent->urld.last_visit > expiry) &&
				(parent->urld.visits > 0)))
			/* expired */
			return;

		fprintf(fp, "%s\n", parent->scheme);

		if (parent->port)
			fprintf(fp,"%d\n", parent->port);
		else
			fprintf(fp, "\n");

		fprintf(fp, "%s\n", *path);

		/** \todo handle fragments? */

		fprintf(fp, "%i\n%i\n%i\n", parent->urld.visits,
				(int)parent->urld.last_visit,
				(int)parent->urld.type);

#ifdef riscos
		if (parent->thumb)
			fprintf(fp, "%s\n", parent->thumb->filename);
		else
			fprintf(fp, "\n");
#else
		fprintf(fp, "\n");
#endif

		if (parent->urld.title) {
			char *s = parent->urld.title;
			for (i = 0; s[i] != '\0'; i++)
				if (s[i] < 32)
					s[i] = ' ';
			for (--i; ((i > 0) && (s[i] == ' ')); i--)
					s[i] = '\0';
			fprintf(fp, "%s\n", parent->urld.title);
		} else
			fprintf(fp, "\n");
	}

	for (p = parent->children; p; p = p->next) {
		int len = *path_used + strlen(p->segment) + 1;
		if (*path_alloc < len) {
			char *temp = realloc(*path,
				(len > 64) ? len : *path_alloc + 64);
			if (!temp)
				return;
			*path = temp;
			*path_alloc = (len > 64) ? len : *path_alloc + 64;
		}

		strcat(*path, p->segment);
		if (p->children) {
			strcat(*path, "/");
		} else {
			len -= 1;
		}

		*path_used = len;

		urldb_write_paths(p, host, fp, path, path_alloc, path_used,
				expiry);

		/* restore path to its state on entry to this function */
		*path_used = pused;
		(*path)[pused - 1] = '\0';
	}
}

/**
 * Insert an URL into the database
 *
 * \param url Absolute URL to insert
 * \return true on success, false otherwise
 */
bool urldb_add_url(const char *url)
{
	struct host_part *h;
	struct path_data *p;
	char *fragment = NULL, *host, *plq, *scheme, *colon, *urlt;
	unsigned short port;
	url_func_result ret;

	assert(url);

	urlt = strdup(url);
	if (!urlt)
		return false;

	host = strchr(urlt, '#');
	if (host) {
		*host = '\0';
		fragment = strdup(host+1);
		if (!fragment) {
			free(urlt);
			return false;
		}
	}

	/* extract host */
	ret = url_host(url, &host);
	if (ret != URL_FUNC_OK) {
		free(fragment);
		free(urlt);
		return false;
	}

	/* extract path, leafname, query */
	ret = url_plq(url, &plq);
	if (ret != URL_FUNC_OK) {
		free(host);
		free(fragment);
		free(urlt);
		return false;
	}

	/* extract scheme */
	ret = url_scheme(url, &scheme);
	if (ret != URL_FUNC_OK) {
		free(plq);
		free(host);
		free(fragment);
		free(urlt);
		return false;
	}

	colon = strrchr(host, ':');
	if (!colon) {
		port = 0;
	} else {
		*colon = '\0';
		port = atoi(colon + 1);
	}

	/* Get host entry */
	if (strcasecmp(scheme, "file") == 0)
		h = urldb_add_host("localhost");
	else
		h = urldb_add_host(host);
	if (!h) {
		free(scheme);
		free(plq);
		free(host);
		free(fragment);
		free(urlt);
		return false;
	}

	/* Get path entry */
	p = urldb_add_path(scheme, port, h, plq, fragment, urlt);
	if (!p) {
		return false;
	}

	free(scheme);
	free(plq);
	free(host);
	free(fragment);
	free(urlt);

	return true;
}

/**
 * Set an URL's title string, replacing any existing one
 *
 * \param url The URL to look for
 * \param title The title string to use (copied)
 */
void urldb_set_url_title(const char *url, const char *title)
{
	struct path_data *p;
	char *temp;

	assert(url && title);

	p = urldb_find_url(url);
	if (!p)
		return;

	temp = strdup(title);
	if (!temp)
		return;

	free(p->urld.title);
	p->urld.title = temp;
}

/**
 * Set an URL's content type
 *
 * \param url The URL to look for
 * \param type The type to set
 */
void urldb_set_url_content_type(const char *url, content_type type)
{
	struct path_data *p;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return;

	p->urld.type = type;
}

/**
 * Update an URL's visit data
 *
 * \param url The URL to update
 */
void urldb_update_url_visit_data(const char *url)
{
	struct path_data *p;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return;

	p->urld.last_visit = time(NULL);
	p->urld.visits++;
}

/**
 * Reset an URL's visit statistics
 *
 * \param url The URL to reset
 */
void urldb_reset_url_visit_data(const char *url)
{
	struct path_data *p;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return;

	p->urld.last_visit = (time_t)0;
	p->urld.visits = 0;
}


/**
 * Find data for an URL.
 *
 * \param url Absolute URL to look for
 * \return Pointer to result struct, or NULL
 */
const struct url_data *urldb_get_url_data(const char *url)
{
	struct path_data *p;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return NULL;

	return (struct url_data *)&p->urld;
}

/**
 * Extract an URL from the db
 *
 * \param url URL to extract
 * \return Pointer to database's copy of URL or NULL if not found
 */
const char *urldb_get_url(const char *url)
{
	struct path_data *p;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return NULL;

	return p->url;
}

/**
 * Look up authentication details in database
 *
 * \param url Absolute URL to search for
 * \return Pointer to authentication details, or NULL if not found
 */
const char *urldb_get_auth_details(const char *url)
{
	struct path_data *p, *q = NULL;

	assert(url);

	/* add to the db, so our lookup will work */
	urldb_add_url(url);

	p = urldb_find_url(url);
	if (!p)
		return NULL;

	/* Check for any auth details attached to this node */
	if (p && p->auth.realm && p->auth.auth)
		return p->auth.auth;

	/* Now consider ancestors */
	for (; p; p = p->parent) {
		/* The parent path entry is stored hung off the
		 * parent entry with an empty (not NULL) segment string.
		 * We look for this here.
		 */
		for (q = p->children; q; q = q->next) {
			if (strlen(q->segment) == 0)
				break;
		}

		if (q && q->auth.realm && q->auth.auth)
			break;
	}

	if (!q)
		return NULL;

	return q->auth.auth;
}

/**
 * Retrieve certificate verification permissions from database
 *
 * \param url Absolute URL to search for
 * \return true to permit connections to hosts with invalid certificates,
 * false otherwise.
 */
bool urldb_get_cert_permissions(const char *url)
{
	struct path_data *p;
	struct host_part *h;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return false;

	for (; p && p->parent; p = p->parent)
		/* do nothing */;

	h = (struct host_part *)p;

	return h->permit_invalid_certs;
}

/**
 * Set authentication data for an URL
 *
 * \param url The URL to consider
 * \param realm The authentication realm
 * \param auth The authentication details (in form username:password)
 */
void urldb_set_auth_details(const char *url, const char *realm,
		const char *auth)
{
	struct path_data *p;
	char *urlt, *t1, *t2;

	assert(url && realm && auth);

	urlt = strdup(url);
	if (!urlt)
		return;

	/* strip leafname from URL */
	t1 = strrchr(urlt, '/');
	if (t1) {
		*(t1 + 1) = '\0';
	}

	/* add url, in case it's missing */
	urldb_add_url(urlt);

	p = urldb_find_url(urlt);

	free(urlt);

	if (!p)
		return;

	/** \todo search subtree for same realm/auth details
	 * and remove them (as the lookup routine searches up the tree) */

	t1 = strdup(realm);
	t2 = strdup(auth);

	if (!t1 || !t2) {
		free(t1);
		free(t2);
		return;
	}

	free(p->auth.realm);
	free(p->auth.auth);

	p->auth.realm = t1;
	p->auth.auth = t2;
}

/**
 * Set certificate verification permissions
 *
 * \param url URL to consider
 * \param permit Set to true to allow invalid certificates
 */
void urldb_set_cert_permissions(const char *url, bool permit)
{
	struct path_data *p;
	struct host_part *h;

	assert(url);

	/* add url, in case it's missing */
	urldb_add_url(url);

	p = urldb_find_url(url);
	if (!p)
		return;

	for (; p && p->parent; p = p->parent)
		/* do nothing */;

	h = (struct host_part *)p;

	h->permit_invalid_certs = permit;
}

/**
 * Set thumbnail for url, replacing any existing thumbnail
 *
 * \param url Absolute URL to consider
 * \param bitmap Opaque pointer to thumbnail data
 */
void urldb_set_thumbnail(const char *url, struct bitmap *bitmap)
{
	struct path_data *p;

	assert(url && bitmap);

	p = urldb_find_url(url);
	if (!p)
		return;

	if (p->thumb)
		bitmap_destroy(p->thumb);

	p->thumb = bitmap;
}

/**
 * Retrieve thumbnail data for given URL
 *
 * \param url Absolute URL to search for
 * \return Pointer to thumbnail data, or NULL if not found.
 */
const struct bitmap *urldb_get_thumbnail(const char *url)
{
	struct path_data *p;

	assert(url);

	p = urldb_find_url(url);
	if (!p)
		return NULL;

	return p->thumb;
}

/**
 * Iterate over entries in the database which match the given prefix
 *
 * \param prefix Prefix to match
 * \param callback Callback function
 */
void urldb_iterate_partial(const char *prefix,
		bool (*callback)(const char *url,
		const struct url_data *data))
{
	char host[256];
	char buf[260]; /* max domain + "www." */
	const char *slash, *scheme_sep;
	struct search_node *tree;
	const struct host_part *h;

	assert(prefix && callback);

	/* strip scheme */
	scheme_sep = strstr(prefix, "://");
	if (scheme_sep)
		prefix = scheme_sep + 3;

	slash = strchr(prefix, '/');

	if (*prefix >= '0' && *prefix <= '9')
		tree = search_trees[ST_IP];
	else if (isalpha(*prefix))
		tree = search_trees[ST_DN + tolower(*prefix) - 'a'];
	else
		return;

	if (slash) {
		/* if there's a slash in the input, then we can
		 * assume that we're looking for a path */
		char *domain = host;

		snprintf(host, sizeof host, "%.*s", slash - prefix, prefix);

		h = urldb_search_find(tree, host);
		if (!h) {
			int len = slash - prefix;

			if ((len == 1 && tolower(host[0]) != 'w') ||
				(len == 2 && (tolower(host[0]) != 'w' ||
					tolower(host[1]) != 'w')) ||
				(len >= 3 &&
					strncasecmp(host, "www", 3))) {
				snprintf(buf, sizeof buf, "www.%s", host);
				h = urldb_search_find(
					search_trees[ST_DN + 'w' - 'a'],
					buf);
				if (!h)
					return;
				domain = buf;
			} else
				return;
		}

		urldb_iterate_partial_path(&h->paths, slash + 1, callback);

	} else {
		int len = strlen(prefix);

		/* looking for hosts */
		if (!urldb_iterate_partial_host(tree, prefix, callback))
			return;

		if ((len == 1 && tolower(prefix[0]) != 'w') ||
				(len == 2 && (tolower(prefix[0]) != 'w' ||
					tolower(prefix[1]) != 'w')) ||
				(len >= 3 &&
					strncasecmp(prefix, "www", 3))) {
			/* now look for www.prefix */
			snprintf(buf, sizeof buf, "www.%s", prefix);
			if(!urldb_iterate_partial_host(
					search_trees[ST_DN + 'w' - 'a'],
					buf, callback))
				return;
		}
	}
}

/**
 * Partial host iterator (internal)
 *
 * \param root Root of (sub)tree to traverse
 * \param prefix Prefix to match
 * \param callback Callback function
 * \return true to continue, false otherwise
 */
bool urldb_iterate_partial_host(struct search_node *root, const char *prefix,
		bool (*callback)(const char *url,
		const struct url_data *data))
{
	int c;

	assert(root && prefix && callback);

	if (root == &empty)
		return true;

	c = urldb_search_match_prefix(root->data, prefix);

	if (c > 0)
		/* No match => look in left subtree */
		return urldb_iterate_partial_host(root->left, prefix,
				callback);
	else if (c < 0)
		/* No match => look in right subtree */
		return urldb_iterate_partial_host(root->right, prefix,
				callback);
	else {
		/* Match => iterate over l/r subtrees & process this node */
		if (!urldb_iterate_partial_host(root->left, prefix,
				callback))
			return false;

		/* and extract all paths attached to this host */
		if (!urldb_iterate_entries_path(&root->data->paths,
				callback)) {
			return false;
		}

		if (!urldb_iterate_partial_host(root->right, prefix,
				callback))
			return false;
	}

	return true;
}

/**
 * Partial path iterator (internal)
 *
 * \param parent Root of (sub)tree to traverse
 * \param prefix Prefix to match
 * \param callback Callback function
 * \return true to continue, false otherwise
 */
bool urldb_iterate_partial_path(const struct path_data *parent,
		const char *prefix, bool (*callback)(const char *url,
		const struct url_data *data))
{
	const struct path_data *p;
	const char *slash, *end = prefix + strlen(prefix);
	int c;

	slash = strchr(prefix, '/');
	if (!slash)
		slash = end;

	if (slash == prefix && *prefix == '/')
		/* Ignore "//" */
		return true;

	for (p = parent->children; p; p = p->next) {
		if ((c = strncasecmp(p->segment, prefix, slash - prefix)) < 0)
			/* didn't match, but may be more */
			continue;
		else if (c > 0)
			/* no more possible matches */
			break;

		/* prefix matches so far */
		if (slash == end) {
			/* we've run out of prefix, so all
			 * paths below this one match */
			if (!urldb_iterate_entries_path(p, callback))
				return false;
		} else {
			/* more prefix to go => recurse */
			if (!urldb_iterate_partial_path(p, slash + 1,
					callback))
				return false;
		}
	}

	return true;
}

/**
 * Iterate over all entries in database
 *
 * \param callback Function to callback for each entry
 */
void urldb_iterate_entries(bool (*callback)(const char *url,
		const struct url_data *data))
{
	int i;

	assert(callback);

	for (i = 0; i < NUM_SEARCH_TREES; i++) {
		if (!urldb_iterate_entries_host(search_trees[i],
				callback))
			break;
	}
}

/**
 * Host data iterator (internal)
 *
 * \param parent Root of subtree to iterate over
 * \param callback Callback function
 * \return true to continue, false otherwise
 */
bool urldb_iterate_entries_host(struct search_node *parent,
		bool (*callback)(const char *url,
		const struct url_data *data))
{
	if (parent == &empty)
		return true;

	if (!urldb_iterate_entries_host(parent->left, callback))
		return false;

	if (!urldb_iterate_entries_path(&parent->data->paths, callback)) {
		return false;
	}

	if (!urldb_iterate_entries_host(parent->right, callback))
		return false;

	return true;
}

/**
 * Path data iterator (internal)
 *
 * \param parent Root of subtree to iterate over
 * \param callback Callback function to call
 * \return true to continue, false otherwise
 */
bool urldb_iterate_entries_path(const struct path_data *parent,
		bool (*callback)(const char *url,
		const struct url_data *data))
{
	const struct path_data *p;

	if (!parent->children) {
		/* leaf node */

		/* All leaf nodes in the path tree should have an URL
		 * attached to them. If this is not the case, it indicates
		 * that there's a bug in the file loader/URL insertion code.
		 * Therefore, assert this here. */
		assert(parent->url);

		/** \todo handle fragments? */

		if (!callback(parent->url,
				(const struct url_data *) &parent->urld))
			return false;
	}

	for (p = parent->children; p; p = p->next) {
		if (!urldb_iterate_entries_path(p, callback))
			return false;
	}

	return true;
}

/**
 * Add a host node to the tree
 *
 * \param part Host segment to add (or whole IP address) (copied)
 * \param parent Parent node to add to
 * \return Pointer to added node, or NULL on memory exhaustion
 */
struct host_part *urldb_add_host_node(const char *part,
		struct host_part *parent)
{
	struct host_part *d;

	assert(part && parent);

	d = calloc(1, sizeof(struct host_part));
	if (!d)
		return NULL;

	d->part = strdup(part);
	if (!d->part) {
		free(d);
		return NULL;
	}

	d->next = parent->children;
	if (parent->children)
		parent->children->prev = d;
	d->parent = parent;
	parent->children = d;

	return d;
}

/**
 * Add a host to the database, creating any intermediate entries
 *
 * \param host Hostname to add
 * \return Pointer to leaf node, or NULL on memory exhaustion
 */
struct host_part *urldb_add_host(const char *host)
{
	struct host_part *d = (struct host_part *) &db_root, *e;
	struct search_node *s;
	char buf[256]; /* 256 bytes is sufficient - domain names are
			* limited to 255 chars. */
	char *part;

	assert(host);

	if (*(host) >= '0' && *(host) <= '9') {
		/* Host is an IP, so simply add as TLD */

		/* Check for existing entry */
		for (e = d->children; e; e = e->next)
			if (strcasecmp(host, e->part) == 0)
				/* found => return it */
				return e;

		d = urldb_add_host_node(host, d);

		s = urldb_search_insert(search_trees[ST_IP], d);
		if (!s) {
			/* failed */
			d = NULL;
		} else {
			search_trees[ST_IP] = s;
		}

		return d;
	}

	/* Copy host string, so we can corrupt it */
	strncpy(buf, host, sizeof buf);
	buf[sizeof buf - 1] = '\0';

	/* Process FQDN segments backwards */
	do {
		part = strrchr(buf, '.');
		if (!part) {
			/* last segment */
			/* Check for existing entry */
			for (e = d->children; e; e = e->next)
				if (strcasecmp(buf, e->part) == 0)
					break;

			if (e) {
				d = e;
			} else {
				d = urldb_add_host_node(buf, d);
			}

			/* And insert into search tree */
			if (d) {
				if (isalpha(*buf)) {
					struct search_node **r;
					r = &search_trees[
						tolower(*buf) - 'a' + ST_DN];

					s = urldb_search_insert(*r, d);
					if (!s) {
						/* failed */
						d = NULL;
					} else {
						*r = s;
					}
				} else {
					d = NULL;
				}
			}
			break;
		}

		/* Check for existing entry */
		for (e = d->children; e; e = e->next)
			if (strcasecmp(part + 1, e->part) == 0)
				break;

		d = e ? e : urldb_add_host_node(part + 1, d);
		if (!d)
			break;

		*part = '\0';
	} while (1);

	return d;
}

/**
 * Add a path node to the tree
 *
 * \param scheme URL scheme associated with path (copied)
 * \param port Port number on host associated with path
 * \param segment Path segment to add (copied)
 * \param fragment URL fragment (copied), or NULL
 * \param parent Parent node to add to
 * \return Pointer to added node, or NULL on memory exhaustion
 */
struct path_data *urldb_add_path_node(const char *scheme, unsigned int port,
		const char *segment, const char *fragment,
		struct path_data *parent)
{
	struct path_data *d, *e;

	assert(scheme && segment && parent);

	d = calloc(1, sizeof(struct path_data));
	if (!d)
		return NULL;

	d->scheme = strdup(scheme);
	if (!d->scheme) {
		free(d);
		return NULL;
	}

	d->port = port;

	d->segment = strdup(segment);
	if (!d->segment) {
		free(d->scheme);
		free(d);
		return NULL;
	}

	if (fragment) {
		if (!urldb_add_path_fragment(d, fragment)) {
			free(d->segment);
			free(d->scheme);
			free(d);
			return NULL;
		}
	}

	for (e = parent->children; e; e = e->next)
		if (strcmp(e->segment, d->segment) > 0)
			break;

	if (e) {
		d->prev = e->prev;
		d->next = e;
		if (e->prev)
			e->prev->next = d;
		else
			parent->children = d;
		e->prev = d;
	} else if (!parent->children) {
		d->prev = d->next = NULL;
		parent->children = parent->last = d;
	} else {
		d->next = NULL;
		d->prev = parent->last;
		parent->last->next = d;
		parent->last = d;
	}
	d->parent = parent;

	return d;
}

/**
 * Add a path to the database, creating any intermediate entries
 *
 * \param scheme URL scheme associated with path
 * \param port Port number on host associated with path
 * \param host Host tree node to attach to
 * \param path Absolute path to add
 * \param fragment URL fragment, or NULL
 * \param url_no_frag URL, without fragment
 * \return Pointer to leaf node, or NULL on memory exhaustion
 */
struct path_data *urldb_add_path(const char *scheme, unsigned int port,
		const struct host_part *host, const char *path,
		const char *fragment, const char *url_no_frag)
{
	struct path_data *d, *e;
	char *buf;
	char *segment, *slash;

	assert(scheme && host && path && url_no_frag);

	d = (struct path_data *) &host->paths;

	/* Copy path string, so we can corrupt it */
	buf = malloc(strlen(path) + 1);
	if (!buf)
		return NULL;

	/* + 1 to strip leading '/' */
	strcpy(buf, path + 1);

	segment = buf;

	/* Process path segments */
	do {
		slash = strchr(segment, '/');
		if (!slash) {
			/* last segment */
			/* look for existing entry */
			for (e = d->children; e; e = e->next)
				if (strcmp(segment, e->segment) == 0 &&
						strcasecmp(scheme,
						e->scheme) == 0 &&
						e->port == port)
					break;

			d = e ? urldb_add_path_fragment(e, fragment) :
					urldb_add_path_node(scheme, port,
					segment, fragment, d);
			break;
		}

		*slash = '\0';

		/* look for existing entry */
		for (e = d->children; e; e = e->next)
			if (strcmp(segment, e->segment) == 0 &&
					strcasecmp(scheme, e->scheme) == 0 &&
					e->port == port)
				break;

		d = e ? e : urldb_add_path_node(scheme, port, segment,
				NULL, d);
		if (!d)
			break;

		segment = slash + 1;
	} while (1);

	free(buf);

	if (d && !d->url) {
		/* Insert URL */
		d->url = strdup(url_no_frag);
		if (!d->url)
			return NULL;
	}

	return d;
}

/**
 * Fragment comparator callback for qsort
 */
int urldb_add_path_fragment_cmp(const void *a, const void *b)
{
	return strcasecmp(*((const char **) a), *((const char **) b));
}

/**
 * Add a fragment to a path segment
 *
 * \param segment Path segment to add to
 * \param fragment Fragment to add (copied), or NULL
 * \return segment or NULL on memory exhaustion
 */
struct path_data *urldb_add_path_fragment(struct path_data *segment,
		const char *fragment)
{
	char **temp;

	assert(segment);

	/* If no fragment, this function is a NOP
	 * This may seem strange, but it makes the rest
	 * of the code cleaner */
	if (!fragment)
		return segment;

	temp = realloc(segment->fragment,
			(segment->frag_cnt + 1) * sizeof(char *));
	if (!temp)
		return NULL;

	segment->fragment = temp;
	segment->fragment[segment->frag_cnt] = strdup(fragment);
	if (!segment->fragment[segment->frag_cnt]) {
		/* Don't free temp - it's now our buffer */
		return NULL;
	}

	segment->frag_cnt++;

	/* We want fragments in alphabetical order, so sort them
	 * It may prove better to insert in alphabetical order instead */
	qsort(segment->fragment, segment->frag_cnt, sizeof (char *),
			urldb_add_path_fragment_cmp);

	return segment;
}

/**
 * Find an URL in the database
 *
 * \param url Absolute URL to find
 * \return Pointer to path data, or NULL if not found
 */
struct path_data *urldb_find_url(const char *url)
{
	const struct host_part *h;
	struct path_data *p;
	struct search_node *tree;
	char *host, *plq, *scheme, *colon;
	const char *domain;
	unsigned short port;
	url_func_result ret;

	assert(url);

	/* extract host */
	ret = url_host(url, &host);
	if (ret != URL_FUNC_OK)
		return NULL;

	/* extract path, leafname, query */
	ret = url_plq(url, &plq);
	if (ret != URL_FUNC_OK) {
		free(host);
		return NULL;
	}

	/* extract scheme */
	ret = url_scheme(url, &scheme);
	if (ret != URL_FUNC_OK) {
		free(plq);
		free(host);
		return NULL;
	}

	colon = strrchr(host, ':');
	if (!colon) {
		port = 0;
	} else {
		*colon = '\0';
		port = atoi(colon + 1);
	}

	/* file urls have no host, so manufacture one */
	if (strcasecmp(scheme, "file") == 0)
		domain = "localhost";
	else
		domain = host;

	if (*domain >= '0' && *domain <= '9')
		tree = search_trees[ST_IP];
	else if (isalpha(*domain))
		tree = search_trees[ST_DN + tolower(*domain) - 'a'];
	else {
		free(plq);
		free(host);
		free(scheme);
		return NULL;
	}

	h = urldb_search_find(tree, domain);
	if (!h) {
		free(plq);
		free(host);
		free(scheme);
		return NULL;
	}

	p = urldb_match_path(&h->paths, plq, scheme, port);

	free(plq);
	free(host);
	free(scheme);

	return p;
}

/**
 * Match a path string
 *
 * \param parent Path (sub)tree to look in
 * \param path The path to search for
 * \param scheme The URL scheme associated with the path
 * \param port The port associated with the path
 * \return Pointer to path data or NULL if not found.
 */
struct path_data *urldb_match_path(const struct path_data *parent,
		const char *path, const char *scheme, unsigned short port)
{
	struct path_data *p;
	const char *slash;

	if (*path == '\0')
		return (struct path_data *)parent;

	slash = strchr(path + 1, '/');
	if (!slash)
		slash = path + strlen(path);

	for (p = parent->children; p; p = p->next) {
		if (strncmp(p->segment, path + 1, slash - path - 1) == 0 &&
				strcmp(p->scheme, scheme) == 0 &&
				p->port == port)
			break;
	}

	if (p) {
		return urldb_match_path(p, slash, scheme, port);
	}

	return NULL;
}

/**
 * Dump URL database to stderr
 */
void urldb_dump(void)
{
	int i;

	urldb_dump_hosts(&db_root);

	for (i = 0; i != NUM_SEARCH_TREES; i++)
		urldb_dump_search(search_trees[i], 0);
}

/**
 * Dump URL database hosts to stderr
 *
 * \param parent Parent node of tree to dump
 */
void urldb_dump_hosts(struct host_part *parent)
{
	struct host_part *h;

	if (parent->part) {
		LOG(("%s", parent->part));

		LOG(("\t%s invalid SSL certs",
			parent->permit_invalid_certs ? "Permits" : "Denies"));
	}

	/* Dump path data */
	urldb_dump_paths(&parent->paths);

	/* and recurse */
	for (h = parent->children; h; h = h->next)
		urldb_dump_hosts(h);
}

/**
 * Dump URL database paths to stderr
 *
 * \param parent Parent node of tree to dump
 */
void urldb_dump_paths(struct path_data *parent)
{
	struct path_data *p;
	unsigned int i;

	if (parent->segment) {
		LOG(("\t%s : %u", parent->scheme, parent->port));

		LOG(("\t\t'%s'", parent->segment));

		for (i = 0; i != parent->frag_cnt; i++)
			LOG(("\t\t\t#%s", parent->fragment[i]));
	}

	/* and recurse */
	for (p = parent->children; p; p = p->next)
		urldb_dump_paths(p);
}

/**
 * Dump search tree
 *
 * \param parent Parent node of tree to dump
 * \param depth Tree depth
 */
void urldb_dump_search(struct search_node *parent, int depth)
{
	const struct host_part *h;
	int i;

	if (parent == &empty)
		return;

	urldb_dump_search(parent->left, depth + 1);

	for (i = 0; i != depth; i++)
			fputc(' ', stderr);

	for (h = parent->data; h; h = h->parent) {
		fprintf(stderr, "%s", h->part);
		if (h->parent && h->parent->parent)
			fputc('.', stderr);
	}

	fputc('\n', stderr);

	urldb_dump_search(parent->right, depth + 1);
}

/**
 * Insert a node into the search tree
 *
 * \param root Root of tree to insert into
 * \param data User data to insert
 * \return Pointer to updated root, or NULL if failed
 */
struct search_node *urldb_search_insert(struct search_node *root,
		const struct host_part *data)
{
	struct search_node *n;

	assert(root && data);

	n = malloc(sizeof(struct search_node));
	if (!n)
		return NULL;

	n->level = 1;
	n->data = data;
	n->left = n->right = &empty;

	root = urldb_search_insert_internal(root, n);

	return root;
}

/**
 * Insert node into search tree
 *
 * \param root Root of (sub)tree to insert into
 * \param n Node to insert
 * \return Pointer to updated root
 */
struct search_node *urldb_search_insert_internal(struct search_node *root,
		struct search_node *n)
{
	assert(root && n);

	if (root == &empty) {
		root = n;
	} else {
		int c = urldb_search_match_host(root->data, n->data);

		if (c > 0) {
			root->left = urldb_search_insert_internal(
					root->left, n);
		} else if (c < 0) {
			root->right = urldb_search_insert_internal(
					root->right, n);
		} else {
			/* exact match */
			free(n);
			return root;
		}

		root = urldb_search_skew(root);
		root = urldb_search_split(root);
	}

	return root;
}

/**
 * Delete a node from a search tree
 *
 * \param root Tree to remove from
 * \param data Data to delete
 * \return Updated root of tree
 */
struct search_node *urldb_search_remove(struct search_node *root,
		const struct host_part *data)
{
	static struct search_node *last, *deleted;

	assert(root && data);

	if (root != &empty) {
		int c = urldb_search_match_host(root->data, data);

		last = root;
		if (c > 0) {
			root->left = urldb_search_remove(root->left, data);
		} else {
			deleted = root;
			root->right = urldb_search_remove(root->right, data);
		}
	}

	if (root == last) {
		if (deleted != &empty &&
				urldb_search_match_host(deleted->data,
						data) == 0) {
			deleted->data = last->data;
			deleted = &empty;
			root = root->right;
		}
	} else {
		if (root->left->level < root->level - 1 ||
				root->right->level < root->level - 1) {
			if (root->right->level > --root->level)
				root->right->level = root->level;

			root = urldb_search_skew(root);
			root->right = urldb_search_skew(root->right);
			root->right->right =
				urldb_search_skew(root->right->right);
			root = urldb_search_split(root);
			root->right = urldb_search_split(root->right);
		}
	}

	return root;
}

/**
 * Find a node in a search tree
 *
 * \param root Tree to look in
 * \param host Host to find
 * \return Pointer to host tree node, or NULL if not found
 */
const struct host_part *urldb_search_find(struct search_node *root,
		const char *host)
{
	int c;

	assert(root && host);

	if (root == &empty) {
		return NULL;
	}

	c = urldb_search_match_string(root->data, host);

	if (c > 0)
		return urldb_search_find(root->left, host);
	else if (c < 0)
		return urldb_search_find(root->right, host);
	else
		return root->data;
}

/**
 * Compare a pair of host_parts
 *
 * \param a
 * \param b
 * \return 0 if match, non-zero, otherwise
 */
int urldb_search_match_host(const struct host_part *a,
		const struct host_part *b)
{
	int ret;

	assert(a && b);

	/* traverse up tree to root, comparing parts as we go. */
	for (; a && a != &db_root && b && b != &db_root;
			a = a->parent, b = b->parent)
		if ((ret = strcasecmp(a->part, b->part)) != 0)
			/* They differ => return the difference here */
			return ret;

	/* If we get here then either:
	 *    a) The path lengths differ
	 * or b) The hosts are identical
	 */
	if (a && a != &db_root && (!b || b == &db_root))
		/* len(a) > len(b) */
		return 1;
	else if ((!a || a == &db_root) && b && b != &db_root)
		/* len(a) < len(b) */
		return -1;

	/* identical */
	return 0;
}

/**
 * Compare host_part with a string
 *
 * \param a
 * \param b
 * \return 0 if match, non-zero, otherwise
 */
int urldb_search_match_string(const struct host_part *a,
		const char *b)
{
	const char *end, *dot;
	int plen, ret;

	assert(a && a != &db_root && b);

	if (*b >= '0' && *b <= '9') {
		/* IP address */
		return strcasecmp(a->part, b);
	}

	end = b + strlen(b) + 1;

	while (b < end && a && a != &db_root) {
		dot = strchr(b, '.');
		if (!dot) {
			/* last segment */
			dot = end - 1;
		}

		/* Compare strings (length limited) */
		if ((ret = strncasecmp(a->part, b, dot - b)) != 0)
			/* didn't match => return difference */
			return ret;

		/* The strings matched, now check that the lengths do, too */
		plen = strlen(a->part);

		if (plen > dot - b)
			/* len(a) > len(b) */
			return 1;
		else if (plen < dot - b)
			/* len(a) < len(b) */
			return -1;

		b = dot + 1;
		a = a->parent;
	}

	/* If we get here then either:
	 *    a) The path lengths differ
	 * or b) The hosts are identical
	 */
	if (a && a != &db_root && b >= end)
		/* len(a) > len(b) */
		return 1;
	else if ((!a || a == &db_root) && b < end)
		/* len(a) < len(b) */
		return -1;

	/* Identical */
	return 0;
}

/**
 * Compare host_part with prefix
 *
 * \param a
 * \param b
 * \return 0 if match, non-zero, otherwise
 */
int urldb_search_match_prefix(const struct host_part *a,
		const char *b)
{
	const char *end, *dot;
	int plen, ret;

	assert(a && a != &db_root && b);

	if (*b >= '0' && *b <= '9') {
		/* IP address */
		return strncasecmp(a->part, b, strlen(b));
	}

	end = b + strlen(b) + 1;

	while (b < end && a && a != &db_root) {
		dot = strchr(b, '.');
		if (!dot) {
			/* last segment */
			dot = end - 1;
		}

		/* Compare strings (length limited) */
		if ((ret = strncasecmp(a->part, b, dot - b)) != 0)
			/* didn't match => return difference */
			return ret;

		/* The strings matched */
		if (dot < end - 1) {
			/* Consider segment lengths only in the case
			 * where the prefix contains segments */
			plen = strlen(a->part);
			if (plen > dot - b)
				/* len(a) > len(b) */
				return 1;
			else if (plen < dot - b)
				/* len(a) < len(b) */
				return -1;
		}

		b = dot + 1;
		a = a->parent;
	}

	/* If we get here then either:
	 *    a) The path lengths differ
	 * or b) The hosts are identical
	 */
	if (a && a != &db_root && b >= end)
		/* len(a) > len(b) => prefix matches */
		return 0;
	else if ((!a || a == &db_root) && b < end)
		/* len(a) < len(b) => prefix does not match */
		return -1;

	/* Identical */
	return 0;
}

/**
 * Rotate a subtree right
 *
 * \param root Root of subtree to rotate
 * \return new root of subtree
 */
struct search_node *urldb_search_skew(struct search_node *root)
{
	struct search_node *temp;

	assert(root);

	if (root->left->level == root->level) {
		temp = root->left;
		root->left = temp->right;
		temp->right = root;
		root = temp;
	}

	return root;
}

/**
 * Rotate a node left, increasing the parent's level
 *
 * \param root Root of subtree to rotate
 * \return New root of subtree
 */
struct search_node *urldb_search_split(struct search_node *root)
{
	struct search_node *temp;

	assert(root);

	if (root->right->right->level == root->level) {
		temp = root->right;
		root->right = temp->left;
		temp->left = root;
		root = temp;

		root->level++;
	}

	return root;
}

#ifdef TEST
int main(void)
{
	struct host_part *h;
	struct path_data *p;

	h = urldb_add_host("127.0.0.1");
	if (!h) {
		LOG(("failed adding host"));
		return 1;
	}

	/* Get host entry */
	h = urldb_add_host("netsurf.strcprstskrzkrk.co.uk");
	if (!h) {
		LOG(("failed adding host"));
		return 1;
	}

	/* Get path entry */
	p = urldb_add_path("http", 80, h, "/path/to/resource.htm?a=b", "zz");
	if (!p) {
		LOG(("failed adding path"));
		return 1;
	}

	p = urldb_add_path("http", 80, h, "/path/to/resource.htm?a=b", "aa");
	if (!p) {
		LOG(("failed adding path"));
		return 1;
	}

	p = urldb_add_path("http", 80, h, "/path/to/resource.htm?a=b", "yy");
	if (!p) {
		LOG(("failed adding path"));
		return 1;
	}

	urldb_dump();

	return 0;
}
#endif
