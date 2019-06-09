// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef MAIN_CONFIG_H
#define MAIN_CONFIG_H

#include"log.h"

typedef struct main_config {
  int ulimit;
  log_config log;

} main_config;

void main_config_init(main_config *main_conf);

#endif // MAIN_CONFIG_H
