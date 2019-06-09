// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include"main_config.h"

void main_config_init(main_config *main_conf) {
  main_conf->ulimit = 4096;
  log_config_init(&main_conf->log);
}

