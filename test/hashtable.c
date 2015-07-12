/*
 * Copyright 2015 Vincent Sanders <vince@netsurf-browser.org>
 * Copyright 2006 Rob Kendrick <rjek@netsurf-browser.org>
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
 * Test hash table operations.
 *
 * Implementation taken from original test rig in bloom filter code
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>

#include "utils/hashtable.h"

/* Tests */

/**
 * Test hash table creation
 *
 * Create a hash table, add a single entry and test for value retival
 * from key.
 *
 */
START_TEST(hashtable_create_test)
{
	struct hash_table *ht;

	ht = hash_create(42);
	ck_assert(ht != NULL);

	hash_destroy(ht);
}
END_TEST

/**
 * Test hash table simple operation
 *
 * Create a hash table, add a single entry and test for failed retival
 * from not present key.
 *
 */
START_TEST(hashtable_negative_test)
{
	struct hash_table *ht;
	bool added;
	const char *res;

	/* create hash */
	ht = hash_create(42);
	ck_assert(ht != NULL);

	/* add entry */
	added = hash_add(ht, "cow", "moo");
	ck_assert(added == true);

	res = hash_get(ht, "sheep");
	ck_assert(res == NULL);

	hash_destroy(ht);
}
END_TEST

/**
 * Test hash table simple operation
 *
 * Create a hash table, add a single entry and test for sucessful
 * retrival of key.
 *
 */
START_TEST(hashtable_positive_test)
{
	struct hash_table *ht;
	bool added;
	const char *res;

	/* create hash */
	ht = hash_create(42);
	ck_assert(ht != NULL);

	/* add entry */
	added = hash_add(ht, "cow", "moo");
	ck_assert(added == true);

	res = hash_get(ht, "cow");
	ck_assert(res != NULL);
	ck_assert_str_eq(res, "moo");

	hash_destroy(ht);
}
END_TEST

/* Suite */

Suite *hashtable_suite(void)
{
	Suite *s;
	TCase *tc_create;

	s = suite_create("hash table filter");

	/* Core API */
	tc_create = tcase_create("Core");

	tcase_add_test(tc_create, hashtable_create_test);
	tcase_add_test(tc_create, hashtable_negative_test);
	tcase_add_test(tc_create, hashtable_positive_test);

	suite_add_tcase(s, tc_create);



	return s;
}

int main(int argc, char **argv)
{
	int number_failed;
	Suite *s;
	SRunner *sr;

	s = hashtable_suite();

	sr = srunner_create(s);
	srunner_run_all(sr, CK_ENV);

	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
