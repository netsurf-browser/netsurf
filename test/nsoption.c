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
 * Tests for user option processing
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <check.h>

#include "utils/errors.h"
#include "utils/log.h"
#include "utils/nsoption.h"

const char *test_choices_path = "test/data/Choices";
const char *test_choices_short_path = "test/data/Choices-short";
const char *test_choices_all_path = "test/data/Choices-all";
const char *test_choices_full_path = "test/data/Choices-full";


nserror gui_options_init_defaults(struct nsoption_s *defaults)
{
	/* Set defaults for absent option strings */
	nsoption_setnull_charp(ca_bundle, strdup("NetSurf:Resources.ca-bundle"));
	nsoption_setnull_charp(cookie_file, strdup("NetSurf:Cookies"));
	nsoption_setnull_charp(cookie_jar, strdup("Cookies"));

	if (nsoption_charp(ca_bundle) == NULL ||
	    nsoption_charp(cookie_file) == NULL ||
	    nsoption_charp(cookie_jar) == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	return NSERROR_OK;
}


/**
 * compare two files contents
 */
static int cmp(const char *f1, const char *f2)
{
	int res = 0;
	FILE *fp1;
	FILE *fp2;
	int ch1;
	int ch2;

	fp1 = fopen(f1, "r");
	if (fp1 == NULL) {
		return -1;
	}
	fp2 = fopen(f2, "r");
	if (fp2 == NULL) {
		fclose(fp1);
		return -1;
	}

	while (res == 0) {
		ch1 = fgetc(fp1);
		ch2 = fgetc(fp2);

		if (ch1 != ch2) {
			res = 1;
		}

		if (ch1 == EOF) {
			break;
		}
	}

	fclose(fp1);
	fclose(fp2);
	return res;
}


/**
 * Test full options session from start to finish
 */
START_TEST(nsoption_session_test)
{
	nserror res;
	int argc = 2;
	char *argv[] = { "nsoption", "--http_proxy_host=fooo", NULL};
	char *outnam;

	res = nsoption_init(gui_options_init_defaults, NULL, NULL);
	ck_assert_int_eq(res, NSERROR_OK);

	/* read from file */
	res = nsoption_read(test_choices_path, NULL);
	ck_assert_int_eq(res, NSERROR_OK);

	/* overlay commandline */
	res = nsoption_commandline(&argc, &argv[0], NULL);
	ck_assert_int_eq(res, NSERROR_OK);

	/* change an option */
	nsoption_set_charp(http_proxy_host, strdup("bar"));

	/* write options out */
	outnam = tmpnam(NULL);
	res = nsoption_write(outnam, NULL, NULL);
	ck_assert_int_eq(res, NSERROR_OK);

	/* check for the correct answer */
	ck_assert_int_eq(cmp(outnam, test_choices_full_path), 0);

	unlink(outnam);

	res = nsoption_finalise(NULL, NULL);
	ck_assert_int_eq(res, NSERROR_OK);

}
END_TEST

TCase *nsoption_session_case_create(void)
{
	TCase *tc;
	tc = tcase_create("Full session");

	tcase_add_test(tc, nsoption_session_test);

	return tc;
}


/**
 * Test dumping option file
 */
START_TEST(nsoption_dump_test)
{
	nserror res;
	char *outnam;
	FILE *fp;

	res = nsoption_read(test_choices_path, NULL);
	ck_assert_int_eq(res, NSERROR_OK);

	outnam = tmpnam(NULL);

	fp = fopen(outnam, "w");
	res = nsoption_dump(fp, NULL);
	fclose(fp);

	ck_assert_int_eq(res, NSERROR_OK);

	ck_assert_int_eq(cmp(outnam, test_choices_all_path), 0);

	unlink(outnam);
}
END_TEST

/**
 * Test writing option file
 */
START_TEST(nsoption_write_test)
{
	nserror res;
	char *outnam;

	res = nsoption_read(test_choices_path, NULL);
	ck_assert_int_eq(res, NSERROR_OK);

	outnam = tmpnam(NULL);

	res = nsoption_write(outnam, NULL, NULL);
	ck_assert_int_eq(res, NSERROR_OK);

	ck_assert_int_eq(cmp(outnam, test_choices_short_path), 0);

	unlink(outnam);
}
END_TEST

/**
 * Test reading option file
 */
START_TEST(nsoption_read_test)
{
	nserror res;
	res = nsoption_read(test_choices_path, NULL);
	ck_assert_int_eq(res, NSERROR_OK);

	ck_assert(nsoption_charp(homepage_url) != NULL);
	ck_assert_str_eq(nsoption_charp(homepage_url), "about:welcome");
}
END_TEST

/**
 * Test commandline string value setting
 */
START_TEST(nsoption_commandline_test)
{
	nserror res;
	int argc = 2;
	char *argv[] = { "nsoption", "--http_proxy_host=fooo", NULL};

	/* commandline */
	res = nsoption_commandline(&argc, &argv[0], NULL);
	ck_assert_int_eq(res, NSERROR_OK);

	ck_assert(nsoption_charp(http_proxy_host) != NULL);
	ck_assert_str_eq(nsoption_charp(http_proxy_host), "fooo");
}
END_TEST

static void nsoption_create(void)
{
	nserror res;
	res = nsoption_init(NULL, NULL, NULL);
	ck_assert_int_eq(res, NSERROR_OK);
}

static void nsoption_teardown(void)
{
	nserror res;

	res = nsoption_finalise(NULL, NULL);
	ck_assert_int_eq(res, NSERROR_OK);
}

TCase *nsoption_case_create(void)
{
	TCase *tc;
	tc = tcase_create("File operations");

	/* ensure options are initialised and finalised for every test */
	tcase_add_unchecked_fixture(tc,
				    nsoption_create,
				    nsoption_teardown);

	tcase_add_test(tc, nsoption_commandline_test);
	tcase_add_test(tc, nsoption_read_test);
	tcase_add_test(tc, nsoption_write_test);
	tcase_add_test(tc, nsoption_dump_test);

	return tc;
}


/**
 * Test finalisation without init
 */
START_TEST(nsoption_api_fini_no_init_test)
{
	nserror res;

	/* attempt to finalise without init */
	res = nsoption_finalise(NULL, NULL);
	ck_assert_int_eq(res, NSERROR_BAD_PARAMETER);
}
END_TEST

/**
 * Test read without path
 */
START_TEST(nsoption_api_read_no_path_test)
{
	nserror res;

	/* read with no path or init */
	res = nsoption_read(NULL, NULL);
	ck_assert_int_eq(res, NSERROR_BAD_PARAMETER);
}
END_TEST

/**
 * Test read without init
 */
START_TEST(nsoption_api_read_no_init_test)
{
	nserror res;

	/* read with path but no init */
	res = nsoption_read(test_choices_path, NULL);
	ck_assert_int_eq(res, NSERROR_BAD_PARAMETER);
}
END_TEST

/**
 * Test write without path
 */
START_TEST(nsoption_api_write_no_path_test)
{
	nserror res;

	/* write with no path or init */
	res = nsoption_write(NULL, NULL, NULL);
	ck_assert_int_eq(res, NSERROR_BAD_PARAMETER);
}
END_TEST

/**
 * Test write without init
 */
START_TEST(nsoption_api_write_no_init_test)
{
	nserror res;

	/* write with path but no init */
	res = nsoption_write(test_choices_path, NULL, NULL);
	ck_assert_int_eq(res, NSERROR_BAD_PARAMETER);

}
END_TEST

/**
 * Test dump without path
 */
START_TEST(nsoption_api_dump_no_path_test)
{
	nserror res;

	/* write with no path or init */
	res = nsoption_dump(NULL, NULL);
	ck_assert_int_eq(res, NSERROR_BAD_PARAMETER);
}
END_TEST

/**
 * Test dump without init
 */
START_TEST(nsoption_api_dump_no_init_test)
{
	nserror res;
	FILE *outf;

	outf = tmpfile();
	ck_assert(outf != NULL);

	/* write with path but no init */
	res = nsoption_dump(outf, NULL);
	ck_assert_int_eq(res, NSERROR_BAD_PARAMETER);

	fclose(outf);
}
END_TEST

/**
 * Test commandline without args
 */
START_TEST(nsoption_api_commandline_no_args_test)
{
	nserror res;
	int argc = 2;
	char *argv[] = { "nsoption", "--http_proxy_host=fooo", NULL};

	/* commandline with no argument count or init */
	res = nsoption_commandline(NULL, &argv[0], NULL);
	ck_assert_int_eq(res, NSERROR_BAD_PARAMETER);

	/* commandline with no argument vector or init */
	res = nsoption_commandline(&argc, NULL, NULL);
	ck_assert_int_eq(res, NSERROR_BAD_PARAMETER);
}
END_TEST

/**
 * Test commandline without init
 */
START_TEST(nsoption_api_commandline_no_init_test)
{
	nserror res;
	int argc = 2;
	char *argv[] = { "nsoption", "--http_proxy_host=fooo", NULL};

	/* write with path but no init */
	res = nsoption_commandline(&argc, &argv[0], NULL);
	ck_assert_int_eq(res, NSERROR_BAD_PARAMETER);
}
END_TEST


/**
 * Test default initialisation and repeated finalisation
 */
START_TEST(nsoption_api_fini_twice_test)
{
	nserror res;
	res = nsoption_init(NULL, NULL, NULL);
	ck_assert_int_eq(res, NSERROR_OK);

	res = nsoption_finalise(NULL, NULL);
	ck_assert_int_eq(res, NSERROR_OK);

	res = nsoption_finalise(NULL, NULL);
	ck_assert_int_eq(res, NSERROR_BAD_PARAMETER);
}
END_TEST


/**
 * Test default initialisation and finalisation
 */
START_TEST(nsoption_api_init_def_test)
{
	nserror res;
	res = nsoption_init(NULL, NULL, NULL);
	ck_assert_int_eq(res, NSERROR_OK);

	res = nsoption_finalise(NULL, NULL);
	ck_assert_int_eq(res, NSERROR_OK);
}
END_TEST

TCase *nsoption_api_case_create(void)
{
	TCase *tc;
	tc = tcase_create("API checks");

	tcase_add_test(tc, nsoption_api_fini_no_init_test);
	tcase_add_test(tc, nsoption_api_read_no_path_test);
	tcase_add_test(tc, nsoption_api_read_no_init_test);
	tcase_add_test(tc, nsoption_api_write_no_path_test);
	tcase_add_test(tc, nsoption_api_write_no_init_test);
	tcase_add_test(tc, nsoption_api_dump_no_path_test);
	tcase_add_test(tc, nsoption_api_dump_no_init_test);
	tcase_add_test(tc, nsoption_api_commandline_no_args_test);
	tcase_add_test(tc, nsoption_api_commandline_no_init_test);
	tcase_add_test(tc, nsoption_api_init_def_test);
	tcase_add_test(tc, nsoption_api_fini_twice_test);

	return tc;
}


Suite *nsoption_suite_create(void)
{
	Suite *s;
	s = suite_create("User options");

	suite_add_tcase(s, nsoption_api_case_create());
	suite_add_tcase(s, nsoption_case_create());
	suite_add_tcase(s, nsoption_session_case_create());

	return s;
}

int main(int argc, char **argv)
{
	int number_failed;
	SRunner *sr;

	sr = srunner_create(nsoption_suite_create());

	srunner_run_all(sr, CK_ENV);

	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
