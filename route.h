// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef ROUTE_H
#define ROUTE_H

typedef struct route {
  unsigned long long id;
  struct route *next;
  char name[200];
  char ssh_command[8192];
} route;

#endif // ROUTE_H

