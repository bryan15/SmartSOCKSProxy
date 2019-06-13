// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdarg.h>
#include<errno.h>

#include"log.h"
#include"log_level.h"
#include"log_file.h"
#include"thread_local.h"
#include"proxy_instance.h"
#include"client_connection.h"
#include"ssh_tunnel.h"

void log_init() {
  tzset();
}

void log_config_init(log_config *conf) {
  conf->level = LOG_LEVEL_INFO;
  conf->file = NULL;
}

void log_write(int level, const char *file, int line, int include_errno, int include_file_line, const char *format, ...) {
  int saved_errno = errno;

  // get thread-local context
  // try to exit quickly if we have no intention of persisting this log message 
  log_config *conf = thread_local_get_log_config();
  if (conf != NULL && conf->level > level) {
    return;
  }
 
  proxy_instance *proxy = thread_local_get_proxy_instance();
  if (conf == NULL && proxy != NULL) {
    conf = &(proxy->log);
  }
  if (conf != NULL && conf->level > level) {
    return;
  }

  ssh_tunnel *ssh = thread_local_get_ssh_tunnel();
  if (conf == NULL && ssh != NULL) {
    conf = &(ssh->log);
  }
  if (conf != NULL && conf->level > level) {
    return;
  }

  service *srv = thread_local_get_service();
  client_connection *con = thread_local_get_client_connection();

  // timestamp
  time_t now_time_t = time(NULL);
  struct tm *now_tm = localtime(&now_time_t);
  char now_buf[100]; 
  int rc=strftime(now_buf, sizeof(now_buf), "%Y-%m-%d %H:%M:%S%z ", now_tm);
  now_buf[rc]=0;

  // message with var-args substitutions etc.
  va_list args;
  char args_buf[1000];
  va_start(args, format);
  vsnprintf(args_buf,sizeof(args_buf),format,args);
  va_end(args);
 
  char *level_buf = log_level_str_upper_fixedwidth(level);

  // now let's assemble our line to print using the bits and pieces we gathered above
  char line_buf[8192];
  char tmp_buf[4096];
  line_buf[0]=0;

  strcat(line_buf,now_buf);

  if (proxy != NULL) {
    snprintf(tmp_buf, sizeof(tmp_buf)-1, "%s ", proxy->name);
    strncat(line_buf,tmp_buf,sizeof(line_buf)-1);
  } 

  if (srv != NULL) {
    char tmp_buf2[1024];
    snprintf(tmp_buf, sizeof(tmp_buf)-1, "%s ", srv->str(srv,tmp_buf2,sizeof(tmp_buf2)-1));
    strncat(line_buf,tmp_buf,sizeof(line_buf)-1);
  } 

  if (con != NULL) {
    snprintf(tmp_buf, sizeof(tmp_buf)-1, "%llu ", con->id);
    strncat(line_buf,tmp_buf,sizeof(line_buf)-1);
  } 

  if (ssh != NULL) {
    snprintf(tmp_buf, sizeof(tmp_buf)-1, "%s ", ssh->name);
    strncat(line_buf,tmp_buf,sizeof(line_buf)-1);
  } 

  strncat(line_buf," ",sizeof(line_buf)-1);

  snprintf(tmp_buf, sizeof(tmp_buf)-1, "%s ", level_buf);
  strncat(line_buf,tmp_buf,sizeof(line_buf)-1);

  if (include_file_line) {
    snprintf(tmp_buf, sizeof(tmp_buf)-1, "in %s line %i: ", file, line);
    strncat(line_buf,tmp_buf,sizeof(line_buf)-1);
  }

  strncat(line_buf,args_buf,sizeof(line_buf)-1);

  if (include_errno) {
    strncat(line_buf,": ",sizeof(line_buf)-1);
    strncat(line_buf,strerror(saved_errno),sizeof(line_buf)-1);
  } 

  strncat(line_buf,"\n",sizeof(line_buf)-1);
  log_file *log = NULL;
  if (conf != NULL) {
    log = conf->file;
  }

  log_file_write(log, line_buf, strlen(line_buf));

  errno=saved_errno;
}

void unexpected_exit(int return_code, char* err_msg) {
  char msg[4096];
  if (errno) {
    snprintf(msg,sizeof(msg)-1,"unexpected exit(%i) (probably means we need to handle an edge case somewhere) %s: %s",return_code, err_msg, strerror(errno));
  } else {
    snprintf(msg,sizeof(msg)-1,"unexpected exit(%i) (probably means we need to handle an edge case somewhere) %s",return_code, err_msg);
  }
  error(msg);
  // make sure this message gets out...
  fprintf(stderr,"%s\n",msg);
  exit(return_code);
}

