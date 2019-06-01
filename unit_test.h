#ifndef UNIT_TEST_H
#define UNIT_TEST_H

extern int tests_run;
extern int tests_success;
extern int tests_fail;

void ut_name(char *name);
int ut_assert(char *test_name,int value);
int ut_assert_true(char *test_name,int value);
int ut_assert_false(char *test_name,int value);
int ut_assert_string_match(char *test_name, char *expected, char *value);
int ut_assert_int_match(char *test_name, int expected, int value);
int ut_assert_long_match(char *test_name, long expected, long value);

#endif // UNIT_TEST_H
