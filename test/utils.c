/*
 * Copyright 2016 Vincent Sanders <vince@netsurf-browser.org>
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
 * Tests for utility functions.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>

#include "utils/string.h"
#include "utils/corestrings.h"

#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))
#define SLEN(x) (sizeof((x)) - 1)

struct test_pairs {
	const unsigned long test;
	const char* res;
};

static const struct test_pairs human_friendly_bytesize_test_vec[] = {
	{ 0, "0.00Bytes" },
	{ 1024, "1024.00Bytes" },
	{ 1025, "1.00kBytes" },
	{ 1048576, "1024.00kBytes" },
	{ 1048577, "1.00MBytes" },
	{ 1073741824, "1024.00MBytes" },
	{ 1073741888, "1024.00MBytes" }, /* spot the rounding error */
	{ 1073741889, "1.00GBytes" },
	{ 2147483648, "2.00GBytes" },
	{ 3221225472, "3.00GBytes" },
	{ 4294967295, "4.00GBytes" },
};

START_TEST(human_friendly_bytesize_test)
{
	char *res_str;
	const struct test_pairs *tst = &human_friendly_bytesize_test_vec[_i];

	res_str = human_friendly_bytesize(tst->test);

	/* ensure result data is correct */
	ck_assert_str_eq(res_str, tst->res);
}
END_TEST

static TCase *human_friendly_bytesize_case_create(void)
{
	TCase *tc;
	tc = tcase_create("Human friendly bytesize");

	tcase_add_loop_test(tc, human_friendly_bytesize_test,
			    0, NELEMS(human_friendly_bytesize_test_vec));

	return tc;
}

struct test_strings {
	const char* test;
	const char* res;
};

static const struct test_strings squash_whitespace_test_vec[] = {
	{ "", "" },
	{ " ", " " },
	{ "    ", " " },
	{ " \n\r\t   ", " " },
	{ " a ", " a " },
	{ " a   b ", " a b " },
};

START_TEST(squash_whitespace_test)
{
	char *res_str;
	const struct test_strings *tst = &squash_whitespace_test_vec[_i];

	res_str = squash_whitespace(tst->test);
	ck_assert(res_str != NULL);

	/* ensure result data is correct */
	ck_assert_str_eq(res_str, tst->res);

	free(res_str);
}
END_TEST

START_TEST(squash_whitespace_api_test)
{
	char *res_str;

	res_str = squash_whitespace(NULL);
	ck_assert(res_str != NULL);

	free(res_str);
}
END_TEST

static TCase *squash_whitespace_case_create(void)
{
	TCase *tc;
	tc = tcase_create("Squash whitespace");

	tcase_add_test_raise_signal(tc, squash_whitespace_api_test, 6);

	tcase_add_loop_test(tc, squash_whitespace_test,
			    0, NELEMS(squash_whitespace_test_vec));

	return tc;
}


START_TEST(corestrings_init_fini_test)
{
	nserror res;

	res = corestrings_init();
	ck_assert_int_eq(res, NSERROR_OK);

	corestrings_fini();
}
END_TEST

START_TEST(corestrings_double_init_test)
{
	nserror res;

	res = corestrings_init();
	ck_assert_int_eq(res, NSERROR_OK);

	res = corestrings_init();
	ck_assert_int_eq(res, NSERROR_OK);

	corestrings_fini();
}
END_TEST

START_TEST(corestrings_double_fini_test)
{
	nserror res;

	res = corestrings_init();
	ck_assert_int_eq(res, NSERROR_OK);

	corestrings_fini();

	corestrings_fini();
}
END_TEST


static TCase *corestrings_case_create(void)
{
	TCase *tc;
	tc = tcase_create("Corestrings");

	tcase_add_test(tc, corestrings_init_fini_test);
	tcase_add_test(tc, corestrings_double_init_test);
	tcase_add_test(tc, corestrings_double_fini_test);

	return tc;
}

static Suite *utils_suite_create(void)
{
	Suite *s;
	s = suite_create("String utils");

	suite_add_tcase(s, human_friendly_bytesize_case_create());
	suite_add_tcase(s, squash_whitespace_case_create());
	suite_add_tcase(s, corestrings_case_create());

	return s;
}

int main(int argc, char **argv)
{
	int number_failed;
	SRunner *sr;

	sr = srunner_create(utils_suite_create());

	srunner_run_all(sr, CK_ENV);

	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
