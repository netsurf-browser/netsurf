#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libwapcaplet/libwapcaplet.h>

#include "desktop/netsurf.h"
#include "utils/log.h"
#include "utils/nsurl.h"

/* desktop/netsurf.h */
bool verbose_log = true;

struct test_pairs {
	const char* test;
	const char* res;
};

static const struct test_pairs create_tests[] = {
	{ "http:",		"http:" },
	{ "http:/",		"http:" },
	{ "http://",		"http:" },
	{ "http:a",		"http://a/" },
	{ "http:a/",		"http://a/" },
	{ "http:a/b",		"http://a/b" },
	{ "http:/a",		"http://a/" },
	{ "http:/a/b",		"http://a/b" },
	{ "http://a",		"http://a/" },
	{ "http://a/b",		"http://a/b" },
	{ "www.example.org",	"http://www.example.org/" },
	{ "www.example.org/x",	"http://www.example.org/x" },
	{ "about:",		"about:" },
	{ "about:blank",	"about:blank" },

	{ "http://www.ns-b.org:8080/",
		"http://www.ns-b.org:8080/" },
	{ "http://user@www.ns-b.org:8080/hello", 
		"http://user@www.ns-b.org:8080/hello" },
	{ "http://user:pass@www.ns-b.org:8080/hello", 
		"http://user:pass@www.ns-b.org:8080/hello" },

	{ "http://www.ns-b.org:80/",
		"http://www.ns-b.org/" },
	{ "http://user@www.ns-b.org:80/hello", 
		"http://user@www.ns-b.org/hello" },
	{ "http://user:pass@www.ns-b.org:80/hello", 
		"http://user:pass@www.ns-b.org/hello" },

	{ "http://www.ns-b.org:/",
		"http://www.ns-b.org/" },
	{ "http://u@www.ns-b.org:/hello", 
		"http://u@www.ns-b.org/hello" },
	{ "http://u:p@www.ns-b.org:/hello", 
		"http://u:p@www.ns-b.org/hello" },

	{ "http:a/",		"http://a/" },
	{ "http:/a/",		"http://a/" },
	{ "http://u@a",		"http://u@a/" },
	{ "http://@a",		"http://a/" },

	{ "mailto:u@a",		"mailto:u@a" },
	{ "mailto:@a",		"mailto:a" },

	{ NULL,			NULL }
};

static const struct test_pairs join_tests[] = {
	/* Normal Examples rfc3986 5.4.1 */
	{ "g:h",		"g:h" },
	{ "g",			"http://a/b/c/g" },
	{ "./g",		"http://a/b/c/g" },
	{ "g/",			"http://a/b/c/g/" },
	{ "/g",			"http://a/g" },
	{ "//g",		"http://g" /* [1] */ "/" },
	{ "?y",			"http://a/b/c/d;p?y" },
	{ "g?y",		"http://a/b/c/g?y" },
	{ "#s",			"http://a/b/c/d;p?q#s" },
	{ "g#s",		"http://a/b/c/g#s" },
	{ "g?y#s",		"http://a/b/c/g?y#s" },
	{ ";x",			"http://a/b/c/;x" },
	{ "g;x",		"http://a/b/c/g;x" },
	{ "g;x?y#s",		"http://a/b/c/g;x?y#s" },
	{ "",			"http://a/b/c/d;p?q" },
	{ ".",			"http://a/b/c/" },
	{ "./",			"http://a/b/c/" },
	{ "..",			"http://a/b/" },
	{ "../",		"http://a/b/" },
	{ "../g",		"http://a/b/g" },
	{ "../..",		"http://a/" },
	{ "../../",		"http://a/" },
	{ "../../g",		"http://a/g" },

	/* Abnormal Examples rfc3986 5.4.2 */
	{ "../../../g",		"http://a/g" },
	{ "../../../../g",	"http://a/g" },

	{ "/./g",		"http://a/g" },
	{ "/../g",		"http://a/g" },
	{ "g.",			"http://a/b/c/g." },
	{ ".g",			"http://a/b/c/.g" },
	{ "g..",		"http://a/b/c/g.." },
	{ "..g",		"http://a/b/c/..g" },

	{ "./../g",		"http://a/b/g" },
	{ "./g/.",		"http://a/b/c/g/" },
	{ "g/./h",		"http://a/b/c/g/h" },
	{ "g/../h",		"http://a/b/c/h" },
	{ "g;x=1/./y",		"http://a/b/c/g;x=1/y" },
	{ "g;x=1/../y",		"http://a/b/c/y" },

	{ "g?y/./x",		"http://a/b/c/g?y/./x" },
	{ "g?y/../x",		"http://a/b/c/g?y/../x" },
	{ "g#s/./x",		"http://a/b/c/g#s/./x" },
	{ "g#s/../x",		"http://a/b/c/g#s/../x" },

	{ "http:g",		"http:g" /* [2] */ },

	/* Extra tests */
	{ " g",			"http://a/b/c/g" },
	{ "g ",			"http://a/b/c/g" },
	{ " g ",		"http://a/b/c/g" },
	{ "http:/b/c",		"http://b/c" },
	{ "http://",		"http:" },
	{ "http:/",		"http:" },
	{ "http:",		"http:" },
	/* [1] Extra slash beyond rfc3986 5.4.1 example, since we're
	 *     testing normalisation in addition to joining */
	/* [2] Using the strict parsers option */
	{ NULL,			NULL }
};

/**
 * Test nsurl
 */
int main(void)
{
	nsurl *base;
	nsurl *joined;
	char *string;
	size_t len;
	const char *url;
	const struct test_pairs *test;
	int passed = 0;
	int count = 0;

	/* Create base URL */
	if (nsurl_create("http://a/b/c/d;p?q", &base) != NSERROR_OK) {
		assert(0 && "Failed to create base URL.");
	}

	if (nsurl_get(base, NSURL_WITH_FRAGMENT, &string, &len) != NSERROR_OK) {
		LOG(("Failed to get string"));
	} else {
		LOG(("Testing nsurl_join with base %s", string));
		free(string);
	}

	for (test = join_tests; test->test != NULL; test++) {
		if (nsurl_join(base, test->test, &joined) != NSERROR_OK) {
			LOG(("Failed to join test URL."));
		} else {
			if (nsurl_get(joined, NSURL_WITH_FRAGMENT,
					&string, &len) !=
					NSERROR_OK) {
				LOG(("Failed to get string"));
			} else {
				if (strcmp(test->res, string) == 0) {
					LOG(("\tPASS: \"%s\"\t--> %s",
						test->test,
						string));
					passed++;
				} else {
					LOG(("\tFAIL: \"%s\"\t--> %s",
						test->test,
						string));
					LOG(("\t\tExpecting: %s",
						test->res));
				}
				free(string);
			}
			nsurl_unref(joined);
		}
		count++;
	}

	nsurl_unref(base);

	/* Create tests */
	LOG(("Testing nsurl_create"));
	for (test = create_tests; test->test != NULL; test++) {
		if (nsurl_create(test->test, &base) != NSERROR_OK) {
			LOG(("Failed to create URL:\n\t\t%s.", test->test));
		} else {
			if (strcmp(nsurl_access(base), test->res) == 0) {
				LOG(("\tPASS: \"%s\"\t--> %s",
					test->test, nsurl_access(base)));
				passed++;
			} else {
				LOG(("\tFAIL: \"%s\"\t--> %s",
					test->test, nsurl_access(base)));
				LOG(("\t\tExpecting %s", test->res));
			}

			nsurl_unref(base);
		}
		count++;
	}

	if (passed == count) {
		LOG(("Testing complete: SUCCESS"));
	} else {
		LOG(("Testing complete: FAILURE"));
		LOG(("Failed %d out of %d", count - passed, count));
	}

	return 0;
}

