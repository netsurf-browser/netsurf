/*
 * Copyright 2006 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2009 John Tytgat <joty@netsurf-browser.org>
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


#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include <curl/curl.h>

#include "image/bitmap.h"
#include "content/content.h"
#include "content/urldb.h"
#include "desktop/cookies.h"
#include "desktop/options.h"
#ifdef riscos
/** \todo lose this */
#include "riscos/bitmap.h"
#endif
#include "utils/log.h"
#include "utils/filename.h"
#include "utils/url.h"
#include "utils/utils.h"

int option_expire_url = 0;
bool verbose_log = true;

bool cookies_schedule_update(const struct cookie_data *data)
{
	return true;
}

void cookies_remove(const struct cookie_data *data)
{
}

void die(const char *error)
{
	printf("die: %s\n", error);
	exit(1);
}


void warn_user(const char *warning, const char *detail)
{
	printf("WARNING: %s %s\n", warning, detail);
}

void bitmap_destroy(void *bitmap)
{
}

char *path_to_url(const char *path)
{
	char *r = malloc(strlen(path) + 7 + 1);

	strcpy(r, "file://");
	strcat(r, path);

	return r;
}

int main(void)
{
	struct host_part *h;
	struct path_data *p;
	const struct url_data *u;
	int i;

	url_init();

	h = urldb_add_host("127.0.0.1");
	if (!h) {
		LOG(("failed adding host"));
		return 1;
	}

	h = urldb_add_host("intranet");
	if (!h) {
		LOG(("failed adding host"));
		return 1;
	}

	p = urldb_add_path("http", 0, h, "/", NULL, NULL, "http://intranet/");
	if (!p) {
		LOG(("failed adding path"));
		return 1;
	}

	urldb_set_url_title("http://intranet/", "foo");

	u = urldb_get_url_data("http://intranet/");
	assert(u && strcmp(u->title, "foo") == 0);

	/* Get host entry */
	h = urldb_add_host("netsurf.strcprstskrzkrk.co.uk");
	if (!h) {
		LOG(("failed adding host"));
		return 1;
	}

	/* Get path entry */
	p = urldb_add_path("http", 0, h, "/path/to/resource.htm", "a=b", "zz",
			"http://netsurf.strcprstskrzkrk.co.uk/path/to/resource.htm?a=b");
	if (!p) {
		LOG(("failed adding path"));
		return 1;
	}

	p = urldb_add_path("http", 0, h, "/path/to/resource.htm", "a=b", "aa",
			"http://netsurf.strcprstskrzkrk.co.uk/path/to/resource.htm?a=b");
	if (!p) {
		LOG(("failed adding path"));
		return 1;
	}

	p = urldb_add_path("http", 0, h, "/path/to/resource.htm", "a=b", "yy",
			"http://netsurf.strcprstskrzkrk.co.uk/path/to/resource.htm?a=b");
	if (!p) {
		LOG(("failed adding path"));
		return 1;
	}

	urldb_set_cookie("mmblah=foo; path=/; expires=Thur, 31-Dec-2099 00:00:00 GMT\r\n", "http://www.minimarcos.org.uk/cgi-bin/forum/Blah.pl?,v=login,p=2", NULL);

	urldb_set_cookie("BlahPW=bar; path=/; expires=Thur, 31-Dec-2099 00:00:00 GMT\r\n", "http://www.minimarcos.org.uk/cgi-bin/forum/Blah.pl?,v=login,p=2", NULL);

	urldb_set_cookie("details=foo|bar|Sun, 03-Jun-2007;expires=Mon, 24-Jul-2006 09:53:45 GMT\r\n", "http://ccdb.cropcircleresearch.com/", NULL);

	urldb_set_cookie("PREF=ID=a:TM=b:LM=c:S=d; path=/; domain=.google.com\r\n", "http://www.google.com/", NULL);

	urldb_set_cookie("test=foo, bar, baz; path=/, quux=blah; path=/", "http://www.bbc.co.uk/", NULL);

//	urldb_set_cookie("a=b; path=/; domain=.a.com", "http://a.com/", NULL);

	urldb_set_cookie("foo=bar;Path=/blah;Secure\r\n", "https://www.foo.com/blah/moose", "https://www.foo.com/blah/moose");

	urldb_get_cookie("https://www.foo.com/blah/wxyzabc");

	/* 1563546 */
	assert(urldb_add_url("http:moodle.org") == false);
	assert(urldb_get_url("http:moodle.org") == NULL);

	/* also 1563546 */
	assert(urldb_add_url("http://a_a/"));
	assert(urldb_get_url("http://a_a/"));

	/* 1597646 */
	if (urldb_add_url("http://foo@moose.com/")) {
		LOG(("added http://foo@moose.com/"));
		assert(urldb_get_url("http://foo@moose.com/") != NULL);
	}

	/* 1535120 */
	assert(urldb_add_url("http://www2.2checkout.com/"));
	assert(urldb_get_url("http://www2.2checkout.com/"));

	/* Numeric subdomains */
	assert(urldb_add_url("http://2.bp.blogspot.com/_448y6kVhntg/TSekubcLJ7I/AAAAAAAAHJE/yZTsV5xT5t4/s1600/covers.jpg"));
	assert(urldb_get_url("http://2.bp.blogspot.com/_448y6kVhntg/TSekubcLJ7I/AAAAAAAAHJE/yZTsV5xT5t4/s1600/covers.jpg"));

	/* Valid path */
	assert(urldb_set_cookie("name=value;Path=/\r\n", "http://www.google.com/", NULL));

	/* Valid path (non-root directory) */
	assert(urldb_set_cookie("name=value;Path=/foo/bar/\r\n", "http://www.example.org/foo/bar/", NULL));

	/* Defaulted path */
	assert(urldb_set_cookie("name=value\r\n", "http://www.example.org/foo/bar/baz/bat.html", NULL));
	assert(urldb_get_cookie("http://www.example.org/foo/bar/baz/quux.htm"));

	/* Defaulted path with no non-leaf path segments */
	assert(urldb_set_cookie("name=value\r\n", "http://no-non-leaf.example.org/index.html", NULL));
	assert(urldb_get_cookie("http://no-non-leaf.example.org/page2.html"));
	assert(urldb_get_cookie("http://no-non-leaf.example.org/"));

	/* Valid path (includes leafname) */
	assert(urldb_set_cookie("name=value;Version=1;Path=/index.cgi\r\n", "http://example.org/index.cgi", NULL));
	assert(urldb_get_cookie("http://example.org/index.cgi"));

	/* Valid path (includes leafname in non-root directory) */
	assert(urldb_set_cookie("name=value;Path=/foo/index.html\r\n", "http://www.example.org/foo/index.html", NULL));
	/* Should _not_ match the above, as the leafnames differ */
	assert(urldb_get_cookie("http://www.example.org/foo/bar.html") == NULL);

	/* Invalid path (contains different leafname) */
	assert(urldb_set_cookie("name=value;Path=/index.html\r\n", "http://example.org/index.htm", NULL) == false);
	
	/* Invalid path (contains leafname in different directory) */
	assert(urldb_set_cookie("name=value;Path=/foo/index.html\r\n", "http://www.example.org/bar/index.html", NULL) == false);

	/* Test partial domain match with IP address failing */
	assert(urldb_set_cookie("name=value;Domain=.foo.org\r\n", "http://192.168.0.1/", NULL) == false);

	/* Test handling of non-domain cookie sent by server (domain part should
	 * be ignored) */
	assert(urldb_set_cookie("foo=value;Domain=blah.com\r\n", "http://www.example.com/", NULL));
	assert(strcmp(urldb_get_cookie("http://www.example.com/"), "foo=value") == 0);

	/* Test handling of domain cookie from wrong host (strictly invalid but
	 * required to support the real world) */
	assert(urldb_set_cookie("name=value;Domain=.example.com\r\n", "http://foo.bar.example.com/", NULL));
	assert(strcmp(urldb_get_cookie("http://www.example.com/"), "foo=value; name=value") == 0);

	/* Test presence of separators in cookie value */
	assert(urldb_set_cookie("name=\"value=foo\\\\bar\\\\\\\";\\\\baz=quux\";Version=1\r\n", "http://www.example.org/", NULL));
	assert(strcmp(urldb_get_cookie("http://www.example.org/"), "$Version=1; name=\"value=foo\\\\bar\\\\\\\";\\\\baz=quux\"") == 0);

	/* Test cookie with blank value */
	assert(urldb_set_cookie("a=\r\n", "http://www.example.net/", NULL));
	assert(strcmp(urldb_get_cookie("http://www.example.net/"), "a=") == 0);

	/* Test specification of multiple cookies in one header */
	assert(urldb_set_cookie("a=b, foo=bar; Path=/\r\n", "http://www.example.net/", NULL));
	assert(strcmp(urldb_get_cookie("http://www.example.net/"), "a=b; foo=bar") == 0);

	/* Test use of separators in unquoted cookie value */
	assert(urldb_set_cookie("foo=moo@foo:blah?moar\\ text\r\n", "http://example.com/", NULL));
	assert(strcmp(urldb_get_cookie("http://example.com/"), "foo=moo@foo:blah?moar\\ text; name=value") == 0);

	/* Test use of unnecessary quotes */
	assert(urldb_set_cookie("foo=\"hello\";Version=1,bar=bat\r\n", "http://example.com/", NULL));
	assert(strcmp(urldb_get_cookie("http://example.com/"), "foo=\"hello\"; bar=bat; name=value") == 0);

	/* Test domain matching in unverifiable transactions */
	assert(urldb_set_cookie("foo=bar; domain=.example.tld\r\n", "http://www.foo.example.tld/", "http://bar.example.tld/"));
	assert(strcmp(urldb_get_cookie("http://www.foo.example.tld/"), "foo=bar") == 0);

	/* Test expiry */
	assert(urldb_set_cookie("foo=bar", "http://expires.com/", NULL));
	assert(strcmp(urldb_get_cookie("http://expires.com/"), "foo=bar") == 0);
	assert(urldb_set_cookie("foo=bar; expires=Thu, 01-Jan-1970 00:00:01 GMT\r\n", "http://expires.com/", NULL));
	assert(urldb_get_cookie("http://expires.com/") == NULL);

	urldb_dump();

	printf("PASS\n");

	return 0;
}

