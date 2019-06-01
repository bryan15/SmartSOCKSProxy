// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdlib.h>

#include"unit_test.h"
#include"log_level.h"

void unit_test_log_level() {
  ut_name("config_file.read_line()");

  ut_assert("invalid log_level_str() 01", log_level_str(-2000) == NULL);  
  ut_assert("invalid log_level_str() 02", log_level_str(2000) == NULL);  
  ut_assert_string_match("log_level_str() 03", "error", log_level_str(LOG_LEVEL_ERROR) );  

  ut_assert_int_match("log_level_from_str() 01", LOG_LEVEL_ERROR, log_level_from_str("error") );  
  ut_assert_int_match("log_level_from_str() 02", LOG_LEVEL_WARN, log_level_from_str("warn") );  
  ut_assert_int_match("log_level_from_str() 03", LOG_LEVEL_WARN, log_level_from_str("WARN") );  
  ut_assert_int_match("log_level_from_str() 04", LOG_LEVEL_INVALID, log_level_from_str("nothing") );  
  ut_assert_int_match("log_level_from_str() 05", LOG_LEVEL_INVALID, log_level_from_str(NULL) );  
}

