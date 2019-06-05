// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdlib.h>

#include"unit_test.h"
#include"string2.h"

void unit_test_string2() {
  ut_name("string2");

  ut_assert("string_ends_with 01", string_ends_with("abcdefg","efg"));  
  ut_assert_false("string_ends_with 02", string_ends_with("abcdefg","ggg"));  
  ut_assert_false("string_ends_with 03", string_ends_with("ggg","abcggg"));  
  ut_assert_false("string_ends_with 04", string_ends_with("ggg","agg"));  
  ut_assert_false("string_ends_with 05", string_ends_with("ggg","aggg"));  
  ut_assert_false("string_ends_with 06", string_ends_with("ggg","abc"));  
}

