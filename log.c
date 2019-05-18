// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/errno.h>
#include<sys/stat.h>
#include<pthread.h>
#include<unistd.h>
#include<stdarg.h>
#include<fcntl.h>

#include"log.h"
#include"thread_local.h"
#include"proxy_instance.h"
#include"client_connection.h"

pthread_mutex_t log_mutex;
logfile_primitive *logfile_primitive_list;

void log_init() {
  logfile_primitive_list = NULL;
  pthread_mutex_init(&log_mutex,NULL);
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

// Not much error checking here. 
// I really assume we can talk to the filesystem :-/
// IMPROVEMENT: better error reporting to the user. Esp. around chmod
void open_logfile(logfile_primitive *lp) {
  int rc;
  rc=open( lp->filename, O_CREAT | O_APPEND | O_RDWR | O_EXLOCK | O_NONBLOCK );
  if (rc < 0) {
    char buf[LOG_LOGFILE_NAME_MAX_LEN+2000];
    sprintf(buf,"open(%s): %s\n",lp->filename, strerror(errno));
    write(STDERR_FILENO,buf,strlen(buf));
    unexpected_exit(1,"open()");
  }
  chmod(lp->filename,0644);
  lp->fd = rc; 
  lp->byte_count = lseek(lp->fd, 0, SEEK_CUR);
}

logfile_primitive *new_logfile_primitive(char *filename, long max_size, long max_rotate) {
  if (filename == NULL) {
    filename = "-";
  }

  // first, do we already have this file in our list?
  for (logfile_primitive *logfile = logfile_primitive_list; logfile; logfile = logfile->next) {
    if (strncmp(logfile->filename, filename, LOG_LOGFILE_NAME_MAX_LEN-1) == 0) {
      logfile->reference_count++;
      return logfile;
    }
  }
  
  // this is a new logfile spec. 
  logfile_primitive *lp = malloc(sizeof(logfile_primitive)); 
  if (!lp) {
    char buf[2000];
    sprintf(buf,"cannot allocate new logfile_primitve; malloc(): %s",strerror(errno));
    write(STDERR_FILENO,buf,strlen(buf));
    unexpected_exit(2,"write()");
  }  

  strncpy(lp->filename,filename,LOG_LOGFILE_NAME_MAX_LEN-1);
  lp->filename[LOG_LOGFILE_NAME_MAX_LEN-1]=0; // paranoia, ensure string is null-terminated. 
  lp->byte_count=0;
  lp->byte_count_max=max_size;
  lp->file_rotate_count=max_rotate;
  lp->reference_count=1;
  if (strncmp(lp->filename,"-",LOG_LOGFILE_NAME_MAX_LEN-1)==0) {
    lp->fd=STDOUT_FILENO;
    lp->can_rotate=0; 
  } else {
    open_logfile(lp);
  }

  // add this logfile_primitive to the list
  lp->next = logfile_primitive_list;
  logfile_primitive_list = lp;

  return lp;
}


log_info *new_log_info(char *filename, int level, long max_size, long max_rotate) {
  log_info *info = malloc(sizeof(log_info));
  if (!info) {
    char buf[2000];
    sprintf(buf,"cannot allocate new log_info; malloc(): %s",strerror(errno));
    write(STDERR_FILENO,buf,strlen(buf));
    unexpected_exit(3,"write()");
  } 
  info->level = level;
  info->logfile = new_logfile_primitive(filename, max_size, max_rotate);
  return info;
}

void log_write(int level, const char *file, int line, int include_errno, int include_file_line, const char *format, ...) {

  int saved_errno = errno;

  // get thread-local context
  proxy_instance *proxy = thread_local_get_proxy_instance();
  service *srv = thread_local_get_service();
  client_connection *con = thread_local_get_client_connection();

  log_info *info = NULL;
  if (proxy) { 
    info = proxy->log;
  }
  logfile_primitive *logfile = NULL;
  if (info) {
    logfile = info->logfile;
  }

  if (info && info->level > level) {
    return;
  }

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
 
  char *level_buf;
  if (level == LOG_TRACE2) level_buf     = "TRACE2"; 
  else if (level == LOG_TRACE) level_buf = "TRACE "; 
  else if (level == LOG_DEBUG) level_buf = "DEBUG "; 
  else if (level == LOG_INFO) level_buf  = "INFO  "; 
  else if (level == LOG_WARN) level_buf  = "WARN  "; 
  else if (level == LOG_ERROR) level_buf = "ERROR "; 
  else level_buf = "UNKNOWN_LOG_LEVEL";

  char line_buf[8192];
  char tmp_buf[4096];
  line_buf[0]=0;

  // now let's assemble our line to print using the bits and pieces created above

  strcat(line_buf,now_buf);

  //if (proxy && logfile && logfile->reference_count > 1) {
  if (proxy && logfile) {
    snprintf(tmp_buf, sizeof(tmp_buf)-1, "%s ", proxy->name);
    strncat(line_buf,tmp_buf,sizeof(line_buf)-1);
  } 

  if (srv && logfile) {
    char tmp_buf2[1024];
    snprintf(tmp_buf, sizeof(tmp_buf)-1, "%s ", srv->str(srv,tmp_buf2,sizeof(tmp_buf2)-1));
    strncat(line_buf,tmp_buf,sizeof(line_buf)-1);
  } 

  if (con != NULL) {
    snprintf(tmp_buf, sizeof(tmp_buf)-1, "%llu ", con->id);
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

  int fd = STDOUT_FILENO;
  if (level == LOG_ERROR) {
    fd=STDERR_FILENO;
  }
  if (logfile) {
    fd = logfile->fd;
  }
  pthread_mutex_lock(&log_mutex);
  write(fd,line_buf,strlen(line_buf));
  if (logfile) {
    info->logfile->byte_count += strlen(line_buf);
  }

  if (logfile && logfile->can_rotate && logfile->byte_count >= logfile->byte_count_max) {
    close(logfile->fd);
    logfile->fd=-1;
    int i;
    for (i=logfile->file_rotate_count-1; i>=0; i--) {
      char oldfile[LOG_LOGFILE_NAME_MAX_LEN];
      char newfile[LOG_LOGFILE_NAME_MAX_LEN];
      if (i>0) {
        sprintf(oldfile,"%s.%i",logfile->filename,i);
      } else {
        strcpy(oldfile,logfile->filename);
      }
      sprintf(newfile,"%s.%i",logfile->filename,i+1);
      int rc;
      rc=rename(oldfile,newfile);
      if (rc<0 && errno != ENOENT) { // ENOENT = oldfile doesn't exist, which is fine in our case here
        char buf[LOG_LOGFILE_NAME_MAX_LEN + 2048];
        sprintf(buf,"open(%s): %s\n",logfile->filename, strerror(errno));
        write(STDERR_FILENO,buf,strlen(buf));
        unexpected_exit(4,"write()");
      }
    }
    open_logfile(logfile); 
  }
  pthread_mutex_unlock(&log_mutex);
  errno=saved_errno;
}


