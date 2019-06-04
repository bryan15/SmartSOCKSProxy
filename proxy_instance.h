// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef PROXY_INSTANCE_H
#define PROXY_INSTANCE_H

#include"service.h"
#include"client_connection.h"
#include"log.h"

#define PROXY_INSTANCE_MAX_NAME_LEN  1024 
#define PROXY_INSTANCE_MAX_LISTENING_PORTS  200 // really? how many do you need?!

typedef struct proxy_instance {
  struct proxy_instance *next;

  char name[PROXY_INSTANCE_MAX_NAME_LEN];
  log_config log;
  service *service_list;
  route_rule *route_rule_list;
  client_connection *client_connection_list;
} proxy_instance;

proxy_instance *new_proxy_instance();
proxy_instance *insert_proxy_instance(proxy_instance *head, proxy_instance *con);
proxy_instance *new_proxy_instance_from_template(proxy_instance *template);
char *proxy_instance_str(proxy_instance *inst, char *buf, int buflen);


#endif // PROXY_INSTANCE_H
