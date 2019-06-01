// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<strings.h>
#include<sys/errno.h>
#include<sys/stat.h>
#include<pthread.h>
#include<unistd.h>
#include<stdarg.h>
#include<fcntl.h>

#include"log.h"
#include"log_level.h"
#include"thread_local.h"
#include"proxy_instance.h"
#include"client_connection.h"

pthread_mutex_t log_file_mutex;

void log_file_init() {
  pthread_mutex_init(&log_file_mutex,NULL);
}


log_file *new_log_file(char *filename) {
  if (filename == NULL) {
    filename = "-";
  }

  log_file *log = malloc(sizeof(log_file)); 
  if (!log) {
    char buf[2000];
    sprintf(buf,"cannot allocate new log_file; malloc(): %s",strerror(errno));
    write(STDERR_FILENO,buf,strlen(buf));
    unexpected_exit(2,"malloc()");
  }  

  strncpy(log->file_name,filename,LOG_FILE_NAME_MAX_LEN-1);
  log->file_name[LOG_FILE_NAME_MAX_LEN-1]=0; // paranoia, ensure string is null-terminated. 
  log->byte_count=0;
  log->byte_count_max=LOG_FILE_DEFAULT_BYTE_COUNT_MAX;
  log->file_rotate_count=LOG_FILE_DEFAULT_FILE_ROTATE_COUNT;
  if (strncmp(log->file_name,"-",LOG_FILE_NAME_MAX_LEN-1)==0) {
    log->can_rotate=0; 
  } else {
    log->can_rotate=1; 
  }
  return log;
}

log_file *insert_log_file(log_file *head, log_file *log) {
  if (log) {
    log->next = head;
  }
  return log; // log is the new head
}

log_file *new_log_file_from_template(log_file *template, char *filename) {
  log_file *log = new_log_file(filename);
  log->byte_count_max = template->byte_count_max;
  log->file_rotate_count = template->file_rotate_count;
  return log;
}

char *log_file_str(log_file *log, char *buf, int buflen) {
  strncpy(buf,log->file_name,buflen);
  return buf;
} 

log_file *find_log_file(log_file *head, char *filename) {
  for (log_file *log = head; log; log = log->next) {
    if (strncmp(log->file_name, filename, LOG_FILE_NAME_MAX_LEN-1) == 0) {
      return log;
    }
  }
  return NULL;
}

void log_file_write(log_file *log, char *buf, int buflen) {
  int fd = STDOUT_FILENO;
  if (log && log->fd >= 0) {
    fd = log->fd;
  }
  pthread_mutex_lock(&log_file_mutex);
  write(fd,buf,buflen);
  if (log) {
    log->byte_count += buflen;
  }

  if (log && log->can_rotate && log->byte_count >= log->byte_count_max) {
    close(log->fd);
    log->fd=-1;
    int i;
    for (i=log->file_rotate_count-1; i>=0; i--) {
      char oldfile[LOG_FILE_NAME_MAX_LEN];
      char newfile[LOG_FILE_NAME_MAX_LEN];
      if (i>0) {
        sprintf(oldfile,"%s.%i",log->file_name,i);
      } else {
        strcpy(oldfile,log->file_name);
      }
      sprintf(newfile,"%s.%i",log->file_name,i+1);
      int rc;
      rc=rename(oldfile,newfile);
      if (rc<0 && errno != ENOENT) { // ENOENT = oldfile doesn't exist, which is fine in our case here
        char buf[LOG_FILE_NAME_MAX_LEN + 2048];
        sprintf(buf,"rename(%s,%s): %s\n",oldfile,newfile, strerror(errno));
        write(STDERR_FILENO,buf,strlen(buf));
        unexpected_exit(4,"rename()");
      }
    }
    log_file_open(log); 
  }
  pthread_mutex_unlock(&log_file_mutex);
}

// Not much error checking here. 
// I really assume we can talk to the filesystem :-/
// IMPROVEMENT: better error reporting to the user. Esp. around chmod
void log_file_open(log_file *log) {
  if (strncmp(log->file_name,"-",LOG_FILE_NAME_MAX_LEN-1)==0) {
    log->fd = STDOUT_FILENO;
    return;
  }  

  int rc;
  rc=open( log->file_name, O_CREAT | O_APPEND | O_RDWR | O_EXLOCK | O_NONBLOCK );
  if (rc < 0) {
    char buf[LOG_FILE_NAME_MAX_LEN+2000];
    sprintf(buf,"open(%s): %s\n",log->file_name, strerror(errno));
    write(STDERR_FILENO,buf,strlen(buf));
    unexpected_exit(1,"open()");
  }
  chmod(log->file_name,0644);
  log->fd = rc; 
  log->byte_count = lseek(log->fd, 0, SEEK_CUR);
}

