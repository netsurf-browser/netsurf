/*
 * Copyright 2011 John Mark Bell <jmb@netsurf-browser.org>
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
 * Test nsurl operations.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>

#include <libwapcaplet/libwapcaplet.h>

#include "utils/corestrings.h"
#include "utils/nsurl.h"

#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))

struct test_pairs {
	const char* test;
	const char* res;
};

struct test_triplets {
	const char* test1;
	const char* test2;
	const char* res;
};

static void netsurf_lwc_iterator(lwc_string *str, void *pw)
{
	fprintf(stderr,
		"[%3u] %.*s",
		str->refcnt,
		(int)lwc_string_length(str),
		lwc_string_data(str));
}

static const struct test_pairs create_tests[] = {
	{ "",			NULL },
	{ "http:",		NULL },
	{ "http:/",		NULL },
	{ "http://",		NULL },
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
};

static const struct test_pairs nice_tests[] = {
	{ "about:",			NULL },
	{ "www.foo.org",		"www_foo_org" },
	{ "www.foo.org/index.html",	"www_foo_org" },
	{ "www.foo.org/default.en",	"www_foo_org" },
	{ "www.foo.org/about",		"about" },
	{ "www.foo.org/about.jpg",	"about.jpg" },
	{ "www.foo.org/moose/index.en",	"moose" },
	{ "www.foo.org/a//index.en",	"www_foo_org" },
	{ "www.foo.org/a//index.en",	"www_foo_org" },
	{ "http://www.f.org//index.en",	"www_f_org" },
};

static const struct test_pairs nice_strip_tests[] = {
	{ "about:",			NULL },
	{ "www.foo.org",		"www_foo_org" },
	{ "www.foo.org/index.html",	"www_foo_org" },
	{ "www.foo.org/default.en",	"www_foo_org" },
	{ "www.foo.org/about",		"about" },
	{ "www.foo.org/about.jpg",	"about" },
	{ "www.foo.org/moose/index.en",	"moose" },
	{ "www.foo.org/a//index.en",	"www_foo_org" },
	{ "www.foo.org/a//index.en",	"www_foo_org" },
	{ "http://www.f.org//index.en",	"www_f_org" },
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
	{ " ",			"http://a/b/c/d;p?q" },
	{ "  ",			"http://a/b/c/d;p?q" },
	{ "/",			"http://a/" },
	{ "  /  ",		"http://a/" },
	{ "  ?  ",		"http://a/b/c/d;p?" },
	{ "  h  ",		"http://a/b/c/h" },
	{ "http://<!--#echo var=", "http://<!--/#echo%20var="},
	/* [1] Extra slash beyond rfc3986 5.4.1 example, since we're
	 *     testing normalisation in addition to joining */
	/* [2] Using the strict parsers option */
};


static const struct test_triplets replace_query_tests[] = {
	{ "http://netsurf-browser.org/?magical=true",
	  "?magical=true&result=win",
	  "http://netsurf-browser.org/?magical=true&result=win"},

	{ "http://netsurf-browser.org/?magical=true#fragment",
	  "?magical=true&result=win",
	  "http://netsurf-browser.org/?magical=true&result=win#fragment"},

	{ "http://netsurf-browser.org/#fragment",
	  "?magical=true&result=win",
	  "http://netsurf-browser.org/?magical=true&result=win#fragment"},

	{ "http://netsurf-browser.org/path",
	  "?magical=true",
	  "http://netsurf-browser.org/path?magical=true"},

};



/**
 * url creation test
 */
START_TEST(nsurl_create_test)
{
	nserror err;
	nsurl *res;
	const struct test_pairs *tst = &create_tests[_i];

	err = nsurl_create(tst->test, &res);
	if (tst->res == NULL) {
		/* result must be invalid */
		ck_assert(err != NSERROR_OK);

	} else {
		/* result must be valid */
		ck_assert(err == NSERROR_OK);

		ck_assert_str_eq(nsurl_access(res), tst->res);

		nsurl_unref(res);
	}
}
END_TEST

/**
 * url nice filename without stripping
 */
START_TEST(nsurl_nice_nostrip_test)
{
	nserror err;
	nsurl *res_url;
	char *res_str;
	const struct test_pairs *tst = &nice_tests[_i];

	/* not testing create, this should always succeed */
	err = nsurl_create(tst->test, &res_url);
	ck_assert(err == NSERROR_OK);

	err = nsurl_nice(res_url, &res_str, false);
	if (tst->res == NULL) {
		/* result must be invalid (bad input) */
		ck_assert(err != NSERROR_OK);
	} else {
		/* result must be valid */
		ck_assert(err == NSERROR_OK);

		ck_assert_str_eq(res_str, tst->res);

		free(res_str);
	}
	nsurl_unref(res_url);

}
END_TEST

/**
 * url nice filename with stripping
 */
START_TEST(nsurl_nice_strip_test)
{
	nserror err;
	nsurl *res_url;
	char *res_str;
	const struct test_pairs *tst = &nice_strip_tests[_i];

	/* not testing create, this should always succeed */
	err = nsurl_create(tst->test, &res_url);
	ck_assert(err == NSERROR_OK);

	err = nsurl_nice(res_url, &res_str, true);
	if (tst->res == NULL) {
		/* result must be invalid (bad input) */
		ck_assert(err != NSERROR_OK);
	} else {
		/* result must be valid */
		ck_assert(err == NSERROR_OK);

		ck_assert_str_eq(res_str, tst->res);

		free(res_str);
	}
	nsurl_unref(res_url);

}
END_TEST

/**
 * replace query
 */
START_TEST(nsurl_replace_query_test)
{
	nserror err;
	nsurl *res_url;
	nsurl *joined;
	const struct test_triplets *tst = &replace_query_tests[_i];

	/* not testing create, this should always succeed */
	err = nsurl_create(tst->test1, &res_url);
	ck_assert(err == NSERROR_OK);

	err = nsurl_replace_query(res_url, tst->test2, &joined);
	if (tst->res == NULL) {
		/* result must be invalid (bad input) */
		ck_assert(err != NSERROR_OK);
	} else {
		/* result must be valid */
		ck_assert(err == NSERROR_OK);

		ck_assert_str_eq(nsurl_access(joined), tst->res);

		nsurl_unref(joined);
	}
	nsurl_unref(res_url);

}
END_TEST

/**
 * url joining
 */
START_TEST(nsurl_join_test)
{
	nserror err;
	nsurl *base_url;
	nsurl *joined;
	char *string;
	size_t len;
	const struct test_pairs *tst = &join_tests[_i];

	/* not testing create, this should always succeed */
	err = nsurl_create("http://a/b/c/d;p?q", &base_url);
	ck_assert(err == NSERROR_OK);

	err = nsurl_join(base_url, tst->test, &joined);
	if (tst->res == NULL) {
		/* result must be invalid (bad input) */
		ck_assert(err != NSERROR_OK);
	} else {
		/* result must be valid */
		ck_assert(err == NSERROR_OK);

		err = nsurl_get(joined, NSURL_WITH_FRAGMENT, &string, &len);
		ck_assert(err == NSERROR_OK);

		ck_assert_str_eq(string, tst->res);

		free(string);
		nsurl_unref(joined);
	}
	nsurl_unref(base_url);

}
END_TEST


static void corestring_create(void)
{
	ck_assert(corestrings_init() == NSERROR_OK);
}

static void corestring_teardown(void)
{
	corestrings_fini();

	lwc_iterate_strings(netsurf_lwc_iterator, NULL);
}

Suite *nsurl_suite(void)
{
	Suite *s;
	TCase *tc_create;
	TCase *tc_nice_nostrip;
	TCase *tc_nice_strip;
	TCase *tc_replace_query;
	TCase *tc_join;

	s = suite_create("nsurl");

	/* url creation */
	tc_create = tcase_create("Create");

	tcase_add_unchecked_fixture(tc_create,
				    corestring_create,
				    corestring_teardown);

	tcase_add_loop_test(tc_create,
			    nsurl_create_test,
			    0, NELEMS(create_tests));
	suite_add_tcase(s, tc_create);

	/* nice filename without strip */
	tc_nice_nostrip = tcase_create("Nice (nostrip)");

	tcase_add_unchecked_fixture(tc_nice_nostrip,
				    corestring_create,
				    corestring_teardown);

	tcase_add_loop_test(tc_nice_nostrip,
			    nsurl_nice_nostrip_test,
			    0, NELEMS(nice_tests));
	suite_add_tcase(s, tc_nice_nostrip);


	/* nice filename with strip */
	tc_nice_strip = tcase_create("Nice (strip)");

	tcase_add_unchecked_fixture(tc_nice_strip,
				    corestring_create,
				    corestring_teardown);

	tcase_add_loop_test(tc_nice_strip,
			    nsurl_nice_strip_test,
			    0, NELEMS(nice_strip_tests));
	suite_add_tcase(s, tc_nice_strip);


	/* replace query */
	tc_replace_query = tcase_create("Replace Query");

	tcase_add_unchecked_fixture(tc_replace_query,
				    corestring_create,
				    corestring_teardown);

	tcase_add_loop_test(tc_replace_query,
			    nsurl_replace_query_test,
			    0, NELEMS(replace_query_tests));
	suite_add_tcase(s, tc_replace_query);

	/* url join */
	tc_join = tcase_create("Join");

	tcase_add_unchecked_fixture(tc_join,
				    corestring_create,
				    corestring_teardown);

	tcase_add_loop_test(tc_join,
			    nsurl_join_test,
			    0, NELEMS(join_tests));
	suite_add_tcase(s, tc_join);

	return s;
}

int main(int argc, char **argv)
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = nsurl_suite();

	sr = srunner_create(s);
	srunner_run_all(sr, CK_ENV);

	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
