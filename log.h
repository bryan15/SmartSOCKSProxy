// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef LOG_H
#define LOG_H

#define LOG_LOGFILE_NAME_MAX_LEN 2048

// logfile can be shared between multiple threads, and tracks
// data specific to the logfile eg: how many bytes we've written to the file. 
typedef struct logfile_primitive {
  struct logfile_primitive *next; 
  char filename[LOG_LOGFILE_NAME_MAX_LEN+1];
  int fd;                  // file descriptor (aka file handle)
  long byte_count;         // how many bytes have we written to this logfile

  int can_rotate;          // true (non-zero) if this logfile requires rotating. As opposed to, say, stdout or a network socket.
  long byte_count_max;     // how many bytes before we rotate the current logfile
  long file_rotate_count;  // how many previous logfiles to keep

  int reference_count;     // how many proxy_instances are writing to this one file?
} logfile_primitive; 

// log_info is unique to each proxy_instance
typedef struct log_info {
  int  level;              // verbosity
  logfile_primitive *logfile;
} log_info;


void log_init();
log_info *new_log_info(char *filename, int level, long max_size, long max_rotate);
void log_write(int level, const char *file, int line, int include_errno, int include_file_line, const char *format, ...);

void unexpected_exit(int return_code, char* err_msg);

//extern int log_level;
//void log_init(char *filename, long byte_count_max, int file_rotate_count);

#define LOG_TRACE2 0
#define LOG_TRACE  1
#define LOG_DEBUG  2
#define LOG_INFO   3
#define LOG_WARN   4
#define LOG_ERROR  5

#define trace2(args...)   log_write(LOG_TRACE2, __FILE__, __LINE__, 0, 1, args)
#define trace(args...)    log_write(LOG_TRACE,  __FILE__, __LINE__, 0, 1, args)
#define debug(args...)    log_write(LOG_DEBUG,  __FILE__, __LINE__, 0, 0, args)
#define info(args...)     log_write(LOG_INFO,   __FILE__, __LINE__, 0, 0, args)
#define warn(args...)     log_write(LOG_WARN,   __FILE__, __LINE__, 0, 0, args)
#define error(args...)    log_write(LOG_ERROR,  __FILE__, __LINE__, 0, 1, args)
#define errorNum(args...) log_write(LOG_ERROR,  __FILE__, __LINE__, 1, 1, args)

#endif // LOG_H
