// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include"log.h"
#include"proxy_instance.h"
#include"ssh_tunnel.h"
#include"unit_test.h"
#include"config_file.h"

void unit_test_config_file_read_line() {
  ut_name("config_file.read_line()");

  // Test read_line()
  int fds[2];
  char buf[100];
  int cr=0;
  int lf=0;
  char *filename="test";
  int rc;

  pipe(fds);
  // write() shouldn't block if we keep this below the size of the kernel buffer. Otherwise, this test will hang.
  char *test_string="01\n02\r03\n\r04\r\n05\n\n06\r\r07\n\r\r08\r\n\n";
  write(fds[1],test_string,strlen(test_string));
  close(fds[1]);

  rc=read_line(fds[0], buf, sizeof(buf), &cr, &lf, filename, 0);
  ut_assert("line 01", rc == 1);
  ut_assert_string_match("line 01 contents", buf, "01");
  rc=read_line(fds[0], buf, sizeof(buf), &cr, &lf, filename, 0);
  ut_assert("line 02", rc == 1);
  ut_assert_string_match("line 02 contents", "02", buf);
  rc=read_line(fds[0], buf, sizeof(buf), &cr, &lf, filename, 0);
  ut_assert("line 03", rc == 1);
  ut_assert_string_match("line 03 contents", "03", buf);
  rc=read_line(fds[0], buf, sizeof(buf), &cr, &lf, filename, 0);
  ut_assert("line 04", rc == 1);
  ut_assert_string_match("line 04 contents", "04", buf);

  rc=read_line(fds[0], buf, sizeof(buf), &cr, &lf, filename, 0);
  ut_assert("line 05", rc == 1);
  ut_assert_string_match("line 05 contents", "05", buf);
  rc=read_line(fds[0], buf, sizeof(buf), &cr, &lf, filename, 0);
  ut_assert("line 05a", rc == 1);
  ut_assert_string_match("line 05a contents", "", buf);

  rc=read_line(fds[0], buf, sizeof(buf), &cr, &lf, filename, 0);
  ut_assert("line 06", rc == 1);
  ut_assert_string_match("line 06 contents", "06", buf);
  rc=read_line(fds[0], buf, sizeof(buf), &cr, &lf, filename, 0);
  ut_assert("line 06a", rc == 1);
  ut_assert_string_match("line 06a contents", "", buf);
    
  rc=read_line(fds[0], buf, sizeof(buf), &cr, &lf, filename, 0);
  ut_assert("line 07", rc == 1);
  ut_assert_string_match("line 07 contents", "07", buf);
  rc=read_line(fds[0], buf, sizeof(buf), &cr, &lf, filename, 0);
  ut_assert("line 07a", rc == 1);
  ut_assert_string_match("line 07a contents", "", buf);

  rc=read_line(fds[0], buf, sizeof(buf), &cr, &lf, filename, 0);
  ut_assert("line 08", rc == 1);
  ut_assert_string_match("line 08 contents", "08", buf);
  rc=read_line(fds[0], buf, sizeof(buf), &cr, &lf, filename, 0);
  ut_assert("line 08a", rc == 1);
  ut_assert_string_match("line 08a contents", "", buf);

  rc=read_line(fds[0], buf, sizeof(buf), &cr, &lf, filename, 0);
  ut_assert("line <eof>", rc == 2);
  ut_assert_string_match("line <eof> contents", "", buf);

  close(fds[0]);
}

void unit_test_config_file_remove_extra_spaces_and_comments_from_config_line() {
  int rc; 
  char buf[100];

  ut_name("config_file.remove_extra_spaces_and_comments_from_config_line()");

  rc=remove_extra_spaces_and_comments_from_config_line("",buf,sizeof(buf));
  ut_assert_string_match("empty string", "", buf);
  ut_assert_int_match("empty string rc",1,rc);

  rc=remove_extra_spaces_and_comments_from_config_line("# comment",buf,sizeof(buf));
  ut_assert_string_match("comment", "", buf);
  ut_assert_int_match("comment rc",1,rc);

  rc=remove_extra_spaces_and_comments_from_config_line("  # comment",buf,sizeof(buf));
  ut_assert_string_match("comment w/ leading space", "", buf);
  ut_assert_int_match("comment w/ leading space rc",1,rc);

  rc=remove_extra_spaces_and_comments_from_config_line("  one two    three # comment",buf,sizeof(buf));
  ut_assert_string_match("token 01", "one two three", buf);
  ut_assert_int_match("token 01 rc",1,rc);

  rc=remove_extra_spaces_and_comments_from_config_line("  one \"two  three\" # comment",buf,sizeof(buf));
  ut_assert_string_match("double quote 01", "one \"two  three\"", buf);
  ut_assert_int_match("double quote 01 rc", 1, rc);

  rc=remove_extra_spaces_and_comments_from_config_line("  one \'two  three\' # comment",buf,sizeof(buf));
  ut_assert_string_match("single quote 01", "one \'two  three\'", buf);
  ut_assert_int_match("single quote 01 rc", 1, rc);

  rc=remove_extra_spaces_and_comments_from_config_line("  one \'two  three # comment",buf,sizeof(buf));
  ut_assert_int_match("unterminated quote 01 rc", 0, rc);

  rc=remove_extra_spaces_and_comments_from_config_line("  one \\\"two  three # comment",buf,sizeof(buf));
  ut_assert_string_match("escape 01", "one \"two three", buf);
  ut_assert_int_match("escape 01 rc", 1, rc);
}

void unit_test_config_file() {
  unit_test_config_file_read_line();
  unit_test_config_file_remove_extra_spaces_and_comments_from_config_line();
}


