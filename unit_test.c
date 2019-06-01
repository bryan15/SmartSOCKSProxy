// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<string.h>

int tests_run=0;
int tests_success=0;
int tests_fail=0;

int ut_stub(char *test_name, int status) {
  tests_run++;
  printf("  %s  %s\n",status?" ok ":"FAIL", test_name);
  if (status) {
    tests_success++;
    return 0;
  }
  tests_fail++;
  return 1;
}

void ut_name(char *name) {
  printf("%s\n",name);
}

int ut_assert(char *test_name,int value) {
  return ut_stub(test_name, value);
}

int ut_assert_true(char *test_name,int value) {
  return ut_stub(test_name, value);
}

int ut_assert_false(char *test_name,int value) {
  return ut_stub(test_name, !value);
}

int ut_assert_string_match(char *test_name, char *expected, char *value) {
  int match = (strcmp(expected,value) == 0);
  int result = ut_stub(test_name, match );
  if (result) { 
    printf("    Expected: '%s'\n",expected);
    printf("    Got:      '%s'\n",value);
  }
  return result;
}

int ut_assert_int_match(char *test_name, int expected, int value) {
  int match = (expected == value);
  int result = ut_stub(test_name, match );
  if (result) { 
    printf("    Expected: '%i'\n",expected);
    printf("    Got:      '%i'\n",value);
  }
  return result;
}

int ut_assert_long_match(char *test_name, long expected, long value) {
  int match = (expected == value);
  int result = ut_stub(test_name, match );
  if (result) { 
    printf("    Expected: '%li'\n",expected);
    printf("    Got:      '%li'\n",value);
  }
  return result;
}

