// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include"log.h"
#include"unit_test.h"
#include"unit_test_log_level.h"
#include"unit_test_config_file.h"

int main(int argc, char **argv) {

  log_init();

  printf("----------\n");
  printf("Unit Tests\n");
  printf("----------\n");

  unit_test_config_file();  
  unit_test_log_level();

  printf("--------------------\n");
  printf("Tests run:   %i\n", tests_run);
  printf("  success:   %i\n", tests_success);
  printf("  failure:   %i\n", tests_fail);

  if (tests_fail) {
    printf("\n*** WARNING *** Unit Tests Failed!\n\n");
  } else {
    printf("\nTESTS PASSED\n\n");
  }

  return tests_fail;
}

