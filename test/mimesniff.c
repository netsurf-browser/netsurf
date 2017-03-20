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
 * Test mime sniffing
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>

#include "utils/utils.h"
#include "utils/corestrings.h"
#include "content/content_factory.h"
#include "content/mimesniff.h"

#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))

struct test_mimetype {
	const uint8_t* data;
	const size_t len;
	lwc_string **mime_type;
	bool safe;
};

/* helpers */
content_type content_factory_type_from_mime_type(lwc_string *mime_type)
{
	content_type type = CONTENT_NONE;
	return type;
}

/* Fixtures */

static void corestring_create(void)
{
	ck_assert(corestrings_init() == NSERROR_OK);
}

/**
 * iterator for any remaining strings in teardown fixture
 */
static void netsurf_lwc_iterator(lwc_string *str, void *pw)
{
	fprintf(stderr,
		"[%3u] %.*s",
		str->refcnt,
		(int)lwc_string_length(str),
		lwc_string_data(str));
}

static void corestring_teardown(void)
{
	corestrings_fini();

	lwc_iterate_strings(netsurf_lwc_iterator, NULL);
}

/* tests */

START_TEST(mimesniff_api_test)
{
	nserror err;
	lwc_string *effective_type;

	/* no header type, no data and sniffing not allowed */
	err = mimesniff_compute_effective_type(NULL,
					       NULL,
					       0,
					       false,
					       false,
					       &effective_type);
	ck_assert(err == NSERROR_NOT_FOUND);

	/* no header type, no data and sniffing allowed */
	err = mimesniff_compute_effective_type(NULL,
					       NULL,
					       0,
					       true,
					       false,
					       &effective_type);
	ck_assert(err == NSERROR_NEED_DATA);

}
END_TEST


static TCase *mimesniff_api_case_create(void)
{
	TCase *tc;
	tc = tcase_create("mimesniff API");


	tcase_add_test(tc, mimesniff_api_test);

	return tc;
}


/* unknown header exact binary type checks in safe context */
#define SIG(s,m,a) { (const uint8_t *)s, SLEN(s), &corestring_lwc_##m, a }
static struct test_mimetype match_unknown_exact_tests[] = {
	SIG("GIF87a", image_gif, true),
	SIG("GIF89a", image_gif, true),
	SIG("\x89PNG\r\n\x1a\n", image_png, true),
	SIG("\xff\xd8\xff", image_jpeg, true),
	SIG("BM", image_bmp, true),
	SIG("\x00\x00\x01\x00", image_vnd_microsoft_icon, true),
	SIG("OggS\x00", application_ogg, true),
	SIG("\x1a\x45\xdf\xa3", video_webm, true),
	SIG("Rar \x1a\x07\x00", application_x_rar_compressed, true),
	SIG("PK\x03\x04", application_zip, true),
	SIG("\x1f\x8b\x08", application_x_gzip, true),
	SIG("%!PS-Adobe-", application_postscript, true),
	SIG("%PDF-", application_pdf, false),
};

static struct test_mimetype match_unknown_riff_tests[] = {
	SIG("RIFF    WEBPVP", image_webp, true),
	SIG("RIFF    WAVE", audio_wave, true),
};

static struct test_mimetype match_unknown_bom_tests[] = {
	SIG("\xfe\xff",text_plain, false),
	SIG("\xff\xfe", text_plain, false),
	SIG("\xef\xbb\xbf", text_plain, false),
};

static struct test_mimetype match_unknown_ws_tests[] = {
	SIG("<?xml", text_xml, false),
	SIG("<!DOCTYPE HTML ", text_html, false),
	SIG("<HTML ", text_html, false),
	SIG("<HEAD ", text_html, false),
	SIG("<SCRIPT ", text_html, false),
	SIG("<IFRAME ", text_html, false),
	SIG("<H1 ", text_html, false),
	SIG("<DIV ", text_html, false),
	SIG("<FONT ", text_html, false),
	SIG("<TABLE ", text_html, false),
	SIG("<A ", text_html, false),
	SIG("<STYLE ", text_html, false),
	SIG("<TITLE ", text_html, false),
	SIG("<B ", text_html, false),
	SIG("<BODY ", text_html, false),
	SIG("<BR ", text_html, false),
	SIG("<P ", text_html, false),
	SIG("<!-- ", text_html, false),
};

static struct test_mimetype match_unknown_mp4_tests[] = {
	SIG("\x00\x00\x00\040ftypisom\x00\x00\x02\x00isomiso2avc1mp41", video_mp4, true),
	SIG("\x00\x00\x00\040ftypmp41\x00\x00\x02\x00isomiso2avc1mp41", video_mp4, true),
};

static struct test_mimetype match_unknown_txtbin_tests[] = {
	SIG("a\nb\tc  ", text_plain, true),
	SIG("a\nb\tc \x01", application_octet_stream, true),
};

#undef SIG

/**
 * exact unknown tests
 *
 * allows return of unsafe type matches
 */
START_TEST(mimesniff_match_unknown_exact_test)
{
	nserror err;
	const struct test_mimetype *tst = &match_unknown_exact_tests[_i];
	lwc_string *effective_type;
	bool match;

	err = mimesniff_compute_effective_type(NULL,
					       tst->data,
					       tst->len,
					       true,
					       false,
					       &effective_type);
	ck_assert(err == NSERROR_OK);

	ck_assert(lwc_string_caseless_isequal(effective_type,
					      *(tst->mime_type),
					      &match) == lwc_error_ok && match);
	lwc_string_unref(effective_type);
}
END_TEST

/**
 * riff test
 */
START_TEST(mimesniff_match_unknown_riff_test)
{
	nserror err;
	const struct test_mimetype *tst = &match_unknown_riff_tests[_i];
	lwc_string *effective_type;
	bool match;

	err = mimesniff_compute_effective_type(NULL,
					       tst->data,
					       tst->len,
					       true,
					       false,
					       &effective_type);
	ck_assert(err == NSERROR_OK);

	ck_assert(lwc_string_caseless_isequal(effective_type,
					      *(tst->mime_type),
					      &match) == lwc_error_ok && match);
	lwc_string_unref(effective_type);
}
END_TEST

/**
 * BOM test
 */
START_TEST(mimesniff_match_unknown_bom_test)
{
	nserror err;
	const struct test_mimetype *tst = &match_unknown_bom_tests[_i];
	lwc_string *effective_type;
	bool match;

	err = mimesniff_compute_effective_type(NULL,
					       tst->data,
					       tst->len,
					       true,
					       false,
					       &effective_type);
	ck_assert(err == NSERROR_OK);

	ck_assert(lwc_string_caseless_isequal(effective_type,
					      *(tst->mime_type),
					      &match) == lwc_error_ok && match);
	lwc_string_unref(effective_type);
}
END_TEST

/**
 * ws test
 */
START_TEST(mimesniff_match_unknown_ws_test)
{
	nserror err;
	const struct test_mimetype *tst = &match_unknown_ws_tests[_i];
	lwc_string *effective_type;
	bool match;

	err = mimesniff_compute_effective_type(NULL,
					       tst->data,
					       tst->len,
					       true,
					       false,
					       &effective_type);
	ck_assert(err == NSERROR_OK);

	ck_assert(lwc_string_caseless_isequal(effective_type,
					      *(tst->mime_type),
					      &match) == lwc_error_ok && match);
	lwc_string_unref(effective_type);
}
END_TEST

/**
 * ws test
 */
START_TEST(mimesniff_match_unknown_mp4_test)
{
	nserror err;
	const struct test_mimetype *tst = &match_unknown_mp4_tests[_i];
	lwc_string *effective_type;
	bool match;

	err = mimesniff_compute_effective_type(NULL,
					       tst->data,
					       tst->len,
					       true,
					       false,
					       &effective_type);
	ck_assert(err == NSERROR_OK);

	ck_assert(lwc_string_caseless_isequal(effective_type,
					      *(tst->mime_type),
					      &match) == lwc_error_ok && match);
	lwc_string_unref(effective_type);
}
END_TEST

/**
 * unknown header text/binary test
 */
START_TEST(mimesniff_match_unknown_txtbin_test)
{
	nserror err;
	const struct test_mimetype *tst = &match_unknown_txtbin_tests[_i];
	lwc_string *effective_type;
	bool match;

	err = mimesniff_compute_effective_type(NULL,
					       tst->data,
					       tst->len,
					       true,
					       false,
					       &effective_type);
	ck_assert(err == NSERROR_OK);

	ck_assert(lwc_string_caseless_isequal(effective_type,
					      *(tst->mime_type),
					      &match) == lwc_error_ok && match);
	lwc_string_unref(effective_type);
}
END_TEST


static TCase *mimesniff_match_unknown_case_create(void)
{
	TCase *tc;
	tc = tcase_create("mimesniff");

	tcase_add_unchecked_fixture(tc,
				    corestring_create,
				    corestring_teardown);

	tcase_add_loop_test(tc,
			    mimesniff_match_unknown_exact_test,
			    0, NELEMS(match_unknown_exact_tests));

	tcase_add_loop_test(tc,
			    mimesniff_match_unknown_riff_test,
			    0, NELEMS(match_unknown_riff_tests));

	tcase_add_loop_test(tc,
			    mimesniff_match_unknown_bom_test,
			    0, NELEMS(match_unknown_bom_tests));

	tcase_add_loop_test(tc,
			    mimesniff_match_unknown_ws_test,
			    0, NELEMS(match_unknown_ws_tests));

	tcase_add_loop_test(tc,
			    mimesniff_match_unknown_mp4_test,
			    0, NELEMS(match_unknown_mp4_tests));

	tcase_add_loop_test(tc,
			    mimesniff_match_unknown_txtbin_test,
			    0, NELEMS(match_unknown_txtbin_tests));

	return tc;
}


START_TEST(mimesniff_unparsable_header_test)
{
	nserror err;
	lwc_string *effective_type;

	/* unparsable header type, no data and sniffing not allowed */
	err = mimesniff_compute_effective_type("badheader",
					       NULL,
					       0,
					       false,
					       false,
					       &effective_type);
	ck_assert_int_eq(err, NSERROR_NOT_FOUND);

	/* unparsable header type, no data and sniffing allowed */
	err = mimesniff_compute_effective_type("badheader",
					       NULL,
					       0,
					       true,
					       false,
					       &effective_type);
	ck_assert_int_eq(err, NSERROR_NEED_DATA);

}
END_TEST


START_TEST(mimesniff_parsable_header_nosniff_test)
{
	nserror err;
	lwc_string *effective_type;
	bool match;

	/* unparsable header type, no data and sniffing not allowed */
	err = mimesniff_compute_effective_type("text/plain",
					       NULL,
					       0,
					       false,
					       false,
					       &effective_type);
	ck_assert(err == NSERROR_OK);

	ck_assert(lwc_string_caseless_isequal(effective_type,
					      corestring_lwc_text_plain,
					      &match) == lwc_error_ok && match);
	lwc_string_unref(effective_type);
}
END_TEST

/* test cases with header mime type */
static TCase *mimesniff_header_case_create(void)
{
	TCase *tc;
	tc = tcase_create("mimesniff header");

	tcase_add_unchecked_fixture(tc,
				    corestring_create,
				    corestring_teardown);

	tcase_add_test(tc, mimesniff_unparsable_header_test);
	tcase_add_test(tc, mimesniff_parsable_header_nosniff_test);

	return tc;
}


static Suite *mimesniff_suite_create(void)
{
	Suite *s;
	s = suite_create("mime sniffing");

	suite_add_tcase(s, mimesniff_api_case_create());
	suite_add_tcase(s, mimesniff_match_unknown_case_create());
	suite_add_tcase(s, mimesniff_header_case_create());

	return s;
}


int main(int argc, char **argv)
{
	int number_failed;
	SRunner *sr;

	sr = srunner_create(mimesniff_suite_create());

	srunner_run_all(sr, CK_ENV);

	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
