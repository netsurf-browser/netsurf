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

#include "content/content_factory.h"
#include "content/mimesniff.h"

/* helpers */
content_type content_factory_type_from_mime_type(lwc_string *mime_type)
{
	content_type type = CONTENT_NONE;
	return type;
}



START_TEST(mimesniff_api_test)
{
	nserror err;
	lwc_string *effective_type;

	err = mimesniff_compute_effective_type(NULL,
					       NULL,
					       0,
					       false,
					       false,
					       &effective_type);
	ck_assert(err == NSERROR_NOT_FOUND);

}
END_TEST


static TCase *mimesniff_case_create(void)
{
	TCase *tc;
	tc = tcase_create("mimesniff");

	tcase_add_test(tc, mimesniff_api_test);

	return tc;
}


static Suite *mimesniff_suite_create(void)
{
	Suite *s;
	s = suite_create("mime sniffing");

	suite_add_tcase(s, mimesniff_case_create());

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
