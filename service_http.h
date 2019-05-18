// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef SERVICE_HTTP_H
#define SERVICE_HTTP_H

#include"service.h"

typedef struct service_http {
  struct service srv;
  char base_dir[4096];
} service_http;

service_http *new_service_http();
service_http *parse_service_http_spec(char *str);

#endif // SERVICE_HTTP_H
