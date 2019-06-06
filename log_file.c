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

// This stuff is too complicated. http://250bpm.com/blog:12
int log_file_lock() {
  int rc;
  do {
    rc = pthread_mutex_lock(&log_file_mutex);
  } while (rc == EINTR); 
  return rc;
}
int log_file_unlock() {
  int rc;
  do {
    rc = pthread_mutex_unlock(&log_file_mutex);
  } while (rc == EINTR); 
  return rc;
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
  log->fd=-1;
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


void log_file_open(log_file *log) {
  // check if already open
  if (log->fd >= 0) {
    return;
  }

  if (strncmp(log->file_name,"-",LOG_FILE_NAME_MAX_LEN-1)==0) {
    log->fd = STDOUT_FILENO;
    return;
  }  
  
  int rc;
  do {
    rc=open( log->file_name, O_CREAT | O_APPEND | O_RDWR | O_EXLOCK | O_NONBLOCK );
  } while (rc < 0 && errno == EINTR);
  if (rc < 0) {
    char buf[LOG_FILE_NAME_MAX_LEN+2000];
    sprintf(buf,"open(%s): %s\n",log->file_name, strerror(errno));
    write(STDERR_FILENO,buf,strlen(buf));
    // hm. maybe let's not exit not exit. We can try again later. 
    // unexpected_exit(1,buf);
  } else {
    log->fd = rc; 
    chmod(log->file_name,0644);
    log->byte_count = lseek(log->fd, 0, SEEK_CUR);
  }
}

void log_file_write2(log_file *log, char *buf, int buflen) {
  write(STDOUT_FILENO,buf,buflen);
}

void log_file_write(log_file *log, char *buf, int buflen) {
  int fd = STDOUT_FILENO;
  if (log_file_lock() != 0) {
    // something wonky with the mutex. Abort! Drop this log message 
    return ;
  }
  if (log) {
    fd = log->fd;
  }
  if (log && fd < 0) {
    log_file_open(log); 
    fd = log->fd;
  }
  if (fd < 0) {
    log_file_unlock();
    char buf[8192];
    if (log) {
      snprintf(buf,sizeof(buf)-1,"cannot write to logfile: %s\n",log->file_name);
    } else {
      snprintf(buf,sizeof(buf)-1,"cannot write to logfile\n");
    }
    buf[sizeof(buf)-1]=0;
    write(STDERR_FILENO,buf,strlen(buf));
    return;
  }
  int rc; 
  do {
    rc = write(fd,buf,buflen);
  } while (rc < 0 && errno == EINTR);
  // if we get other errors, like EAGAIN, simply drop this log message. Sorry, best effort, I tried. 
  if (log) {
    log->byte_count += buflen;
  }
  log_file_unlock();
}


// log_files are somewhat unique in that they're re-used by multiple other objects. 
// Therefore, it's helpful to have a utility function to find a pre-existing 
// log_file, or create one if it doesn't exist. 
log_file *find_or_create_log_file(log_file **head, log_file *template, char *filename) {
  log_file *existing_log = find_log_file(*head, filename);
  if (existing_log) {
    return existing_log;
  }
  log_file *new_log = NULL;
  if (template) {
    new_log = new_log_file_from_template(template, filename);
  } else {
    new_log = new_log_file(filename);
  }
  if (new_log) {
    *head = insert_log_file(*head,new_log);
  }
  return new_log;
}

// Check if this logfile requires rotation
void log_file_rotate(log_file *log) {
  long size; 
  int rc;

  if (!log) {
    return;
  }
  if (!log->can_rotate) {
    return;
  }

  if (log_file_lock() != 0) {
    // something wonky with the mutex. Don't rotate the file this time around.
    return;
  }
  size = log->byte_count;
  log_file_unlock();

  if (size < log->byte_count_max) {
    return;
  }

  // tricky business, if we don't do this right. 
  // This part does a lot of renames. Notices that other
  // threads can continue writing to log->fd even after
  // it's been renamed. That should be fine. 
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
    // Since this function executes from the main thread (only!), it is safe
    // to call regular logging methods. Assuming logging works at all.
    debug("logfile rotate %s -> %s",oldfile,newfile);
    rc=rename(oldfile,newfile);
    if (rc<0 && errno != ENOENT) { // ENOENT = oldfile doesn't exist, which is fine in our case here
      char buf[LOG_FILE_NAME_MAX_LEN + 2048];
      sprintf(buf,"rename(%s,%s): %s\n",oldfile,newfile, strerror(errno));
      write(STDERR_FILENO,buf,strlen(buf));
      unexpected_exit(4,buf);
    }
  }

  // This is where we break ties with the current logfile
  // and force a new one to open. Note! The rename must happen 
  // before we change the file descriptor :D because as soon as
  // we touch the file descriptor, another thread might instantly
  // try to open a new file for writing. What would happen if we 
  // hadn't already renamed the file then? Exactly.
  int fd;
  log_file_lock();
  fd=log->fd;
  log->fd=-1;
  log->byte_count=0;
  log_file_unlock();
  do {
    rc=close(fd); 
  } while (rc<0 && errno == EINTR); 
  if (rc < 0) {
    char buf[2048]; 
    snprintf(buf,2048,"close(%s) = %i, fd=%i, errno = %i %s",log->file_name, rc, fd, errno, strerror(errno));
    unexpected_exit(3,buf);
  }
}



