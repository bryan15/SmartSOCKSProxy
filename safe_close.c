// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<sys/socket.h>
#include<unistd.h>
#include<errno.h>

#include"log.h"
#include"client_connection.h"

// Not super-sure this is correct...
// Basically, on a regularly-closed connection, I want to avoid sending RST 
// to the other end in the event there's still data in the local kernel buffer.
// I want the regular FIN/ACK exchange so nobody gets their socks in a twist. 
// See:  https://stackoverflow.com/questions/4160347/close-vs-shutdown-socket

void safe_close(client_connection *con, int fd) {
  if (fd >=0) {
    int rc;
    do {
      rc = shutdown(fd,SHUT_RDWR);
    } while (rc < 0 && errno == EINTR);
    trace2("shutdown() returned %i",rc);
    do {
      char buf[100];
      rc = read(fd,buf,sizeof(buf));
    } while ((rc > 0) || (rc < 0 && errno == EINTR));
    trace2("read() returned %i",rc);
    do {
      rc=close(fd);
    } while (rc < 0 && errno == EINTR);
    trace2("close() returned %i",rc);
  }
}

