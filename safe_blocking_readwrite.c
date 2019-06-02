// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<strings.h>
#include<stdio.h>

#include"log.h"
#include"safe_blocking_readwrite.h"
#include"thread_local.h"

void byte_dump(int fd, char *prefix, unsigned char *buf, size_t buflen) {
  char line[1000], *ptr;
  char tmp[100];
  int idx;

  // This routine is a little bit heavyweight; if the output
  // is going to be ignored anyways, then don't generate
  // the output.
  log_config *conf = thread_local_get_log_config();
  if (conf != NULL && conf->level > LOG_LEVEL_TRACE2) {
    return;
  }

  ptr=line;
  *ptr=0;
  for (idx=0;idx<buflen;idx++) {
    sprintf(tmp," %02X",buf[idx]);
    strcat(ptr,tmp);
    ptr += strlen(tmp);
    if (idx % 16 == 15) {
      trace2("(%i) %s %s", fd, prefix, line);
      ptr=line;
      *ptr=0;
    }
  }
  if (ptr != line) {
    trace2("(%i) %s %s", fd, prefix, line);
  } 
}

// safe blocking read
int sb_read(int fd, unsigned char *buf, size_t buflen) {
  int rc;
  do {
    rc=read(fd,buf,buflen);
  } while (rc<0 && (errno==EINTR || errno==EAGAIN));
  if (rc<0 && errno != ECONNRESET) {
    int saved_errno=errno;
    errorNum("read()");
    errno=saved_errno;
  }
  if (rc > 0) {
    byte_dump(fd, "<< ",buf,rc);
  } else if (rc == 0) {
    trace("sb_read(): socket unexpectedly closed");
  }
  return rc;
}

// safe blocking read of buflen bytes
int sb_read_len(int fd, unsigned char *buf, size_t buflen) {
  int rc;
  size_t index=0;
  size_t remain=buflen;
  do {
    rc=read(fd,&(buf[index]),remain);
    if (rc>0) {
      byte_dump(fd, "<< ",&(buf[index]),rc);
      remain -= rc;
      index  += rc;
    }
  } while ((rc>0 && remain>0) || (rc<0 && (errno==EINTR || errno==EAGAIN)));
  if (rc<0 && errno != ECONNRESET) {
    int saved_errno=errno;
    errorNum("read()");
    errno=saved_errno;
  } else if (rc == 0) {
    trace("sb_read_len(): socket unexpectedly closed");
  }
  if (rc>0) return index;
  return rc;
}

// safe blocking write of buflen bytes
int sb_write_len(int fd, unsigned char *buf, size_t buflen) {
  int rc;
  size_t index=0;
  size_t remain=buflen;
  do {
    rc=write(fd,&(buf[index]),remain);
    if (rc>=0) {
      byte_dump(fd, ">> ",&(buf[index]),rc);
      remain -= rc;
      index  += rc;
    }
  } while ((rc>=0 && remain>0) || (rc<0 && (errno==EINTR || errno==EAGAIN)));
  if (rc<0 && errno != ECONNRESET) {
    errorNum("write()");
  }
  if (rc>0) return index;
  return rc;
}


