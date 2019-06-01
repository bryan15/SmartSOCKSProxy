// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef LOG_FILE_H
#define LOG_FILE_H

#include"log_level.h"

#define LOG_FILE_NAME_MAX_LEN 2048
#define LOG_FILE_DEFAULT_BYTE_COUNT_MAX (1024*1024*20)
#define LOG_FILE_DEFAULT_FILE_ROTATE_COUNT 4

// Logfiles are keyed off their filename; for a given filename, only one 
// log_file struct exists in memory.
typedef struct log_file {
  struct log_file *next; 
  char file_name[LOG_FILE_NAME_MAX_LEN+1];
  int fd;                  // file descriptor (aka file handle)
  long byte_count;         // how many bytes have we written to this logfile

  int can_rotate;          // true (non-zero) if this logfile requires rotating. As opposed to, say, stdout or a network socket.
  long byte_count_max;     // how many bytes before we rotate the current logfile
  long file_rotate_count;  // how many previous logfiles to keep
} log_file; 


void log_file_init();
log_file *new_log_file(char *filename);
log_file *insert_log_file(log_file *head, log_file *log);
log_file *new_log_file_from_template(log_file *template, char *filename);
char *log_file_str(log_file *log, char *buf, int buflen);
log_file *find_log_file(log_file *head, char *filename);
void log_file_open(log_file *log);
void log_file_write(log_file *log, char *buf, int buflen);

#endif // LOG_FILE_H
