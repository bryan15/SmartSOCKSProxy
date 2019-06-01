#include<stdio.h>
#include"unit_test.h"
#include"unit_test_config_file.h"
#include"log.h"

int main(int argc, char **argv) {

  log_init();

  printf("----------\n");
  printf("Unit Tests\n");
  printf("----------\n");

  unit_test_config_file();  

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

