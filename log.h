// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef LOG_H
#define LOG_H

#include"log_level.h"
#include"log_file.h"

// Each source of logs (main thread, ssh_tunnel, proxy_instance...) has a log_config
// to control how logs are handled. 
typedef struct log_config {
  int  level;
  log_file *file;
} log_config;

void log_init();
void log_config_init(log_config *conf);
void log_write(int level, const char *file, int line, int include_errno, int include_file_line, const char *format, ...);
void unexpected_exit(int return_code, char* err_msg);

#define trace2(args...)   log_write(LOG_LEVEL_TRACE2, __FILE__, __LINE__, 0, 1, args)
#define trace(args...)    log_write(LOG_LEVEL_TRACE,  __FILE__, __LINE__, 0, 1, args)
#define debug(args...)    log_write(LOG_LEVEL_DEBUG,  __FILE__, __LINE__, 0, 0, args)
#define info(args...)     log_write(LOG_LEVEL_INFO,   __FILE__, __LINE__, 0, 0, args)
#define warn(args...)     log_write(LOG_LEVEL_WARN,   __FILE__, __LINE__, 0, 0, args)
#define error(args...)    log_write(LOG_LEVEL_ERROR,  __FILE__, __LINE__, 0, 1, args)
#define errorNum(args...) log_write(LOG_LEVEL_ERROR,  __FILE__, __LINE__, 1, 1, args)

#endif // LOG_H
