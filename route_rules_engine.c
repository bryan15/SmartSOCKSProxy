// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<string.h>

#include"proxy_instance.h"
#include"service.h"
#include"client_connection.h"
#include"route_rule.h"
#include"ssh_tunnel.h"

route_rule *fixme = NULL;

// FIXME TODO: routing engine
// This needs to assign a matching rule. 
int decide_applicable_rule(proxy_instance *proxy, service *srv, client_connection *con) {
  if (!fixme) {
    fixme=new_route_rule();
    strcpy(fixme->file_name,"fixme_static_route");
    fixme->tunnel[0]=ssh_tunnel_direct;
    fixme->tunnel[1]=NULL;
  }
  lock_client_connection(con);
  con->route = fixme;
  unlock_client_connection(con);
  return 1;  
}



