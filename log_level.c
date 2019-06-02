// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdlib.h>
#include<strings.h>

#include"log_level.h"

struct log_level_type {
  int level;
  char *name;
};

struct log_level_type log_levels[] = {
  { LOG_LEVEL_TRACE2, "trace2" },
  { LOG_LEVEL_TRACE, "trace" },
  { LOG_LEVEL_DEBUG, "debug" },
  { LOG_LEVEL_INFO, "info" },
  { LOG_LEVEL_WARN, "warn" },
  { LOG_LEVEL_ERROR, "error" },
  { LOG_LEVEL_INVALID, NULL }
};

char* log_level_str(int level) {
  for (int i=0; log_levels[i].name != NULL; i++) {
    if (log_levels[i].level == level) {
      return log_levels[i].name;
    }
  }
  return NULL;
}

// blah, cheap'n easy way to make a thread-safe version 
char* log_level_str_upper_fixedwidth(int level) {
  switch(level) { 
    case LOG_LEVEL_TRACE2: return "TRACE2";
    case LOG_LEVEL_TRACE:  return "TRACE ";
    case LOG_LEVEL_DEBUG:  return "DEBUG ";
    case LOG_LEVEL_INFO:   return "INFO  ";
    case LOG_LEVEL_WARN:   return "WARN  ";
    case LOG_LEVEL_ERROR:  return "ERROR ";
  }
  return NULL;
}

int log_level_from_str(char *level_str) {
  if (level_str == NULL) {
    return LOG_LEVEL_INVALID;
  }

  for (int i=0; log_levels[i].name != NULL; i++) {
    if (strcasecmp(log_levels[i].name,level_str) == 0) {
      return log_levels[i].level;
    }
  }
  return LOG_LEVEL_INVALID;
}

