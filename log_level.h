// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef LOG_LEVEL_H
#define LOG_LEVEL_H

#define LOG_LEVEL_INVALID (-1)
#define LOG_LEVEL_TRACE2    0
#define LOG_LEVEL_TRACE     1
#define LOG_LEVEL_DEBUG     2
#define LOG_LEVEL_INFO      3
#define LOG_LEVEL_WARN      4
#define LOG_LEVEL_ERROR     5

char *log_level_str(int level);
int log_level_from_str(char *level_str);

#endif // LOG_LEVEL_H
