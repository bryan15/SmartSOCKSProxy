// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<unistd.h>
#include<errno.h>

int thread_messaging_fd;

void thread_msg_init(int fd) {
  thread_messaging_fd = fd;
}

void thread_msg_send(char *msg, int len) {
  if (msg == NULL) return;
  if (len == 0) return;

  int rc;
  do {
    rc=write(thread_messaging_fd,msg,len);
  } while (rc<0 && (errno==EINTR || errno==EAGAIN));
}

