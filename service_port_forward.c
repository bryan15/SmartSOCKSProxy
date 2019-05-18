// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>

#include"log.h"
#include"service.h"
#include"service_port_forward.h"
#include"string2.h"
#include"thread_local.h"
#include"safe_close.h"
#include"socks5.h"
#include"socks_connection.h"
#include"service_socks.h"
#include"service_thread.h"
#include"route_rule.h"
#include"route_rules_engine.h"

char *service_port_forward_str(service_port_forward *fwd, char *buf, int buflen) {
  char local_buf[4096];
  snprintf(buf,buflen-1,"%llu:FWD:%s:%i:%s:%i",
    fwd->srv.id,
    fwd->srv.bind_address,fwd->srv.port,
    fwd->remote_host,
    fwd->remote_port);
  return buf;
}

// returns void* because its started as a new thread, and that's what
// the interface expects. 
void *service_port_forward_connection_handler(void *data) {
  service_thread_setup(data);
  thread_data *tdata = data;
  proxy_instance *proxy = tdata->proxy;
  service_port_forward *fwd=(service_port_forward*)tdata->srv;
  client_connection *con = tdata->con;
  free(data);  // this is our responsibility.

  lock_client_connection(con);
  con->socks_version = 5;
  con->socks_command = con->socks_command_original = SOCKS5_CMD_CONNECT;
  con->socks_address_type = con->socks_address_type_original = SOCKS5_ADDRTYPE_DOMAIN;
  host_id_set_name(&con->dst_host,fwd->remote_host);
  host_id_set_port(&con->dst_host, fwd->remote_port);
  con->dst_host_original=con->dst_host;
  unlock_client_connection(con);

  int ok=1;

  if (ok) {
    ok = decide_applicable_rule(proxy, (service*)fwd, con);
  }
  int failure_type;
  if (ok) {
    ok=socks_connect(proxy, (service*)fwd, con, &failure_type);
  }
  if (ok) {
    ok = socks_connect_shuttle(con);
  }

  service_thread_shutdown(con, ok);
  return NULL;
}


service_port_forward *new_service_port_forward() {
  service_port_forward *fwd = (service_port_forward*) new_service(sizeof(struct service_port_forward), SERVICE_TYPE_PORT_FORWARD);
  fwd->remote_host[0]=0;
  fwd->remote_port=0;
  fwd->srv.str = (void*)&service_port_forward_str; // void* is to bypass incompatible pointer warning. See service.h about polymorphism in C
  fwd->srv.connection_handler = &service_port_forward_connection_handler;
  return fwd;
}

service_port_forward *parse_service_port_forward_spec(char *strIn) {
  char *token; 
  char *strPtr; 
  char *local_copy;
  int okay = 1;
  service_port_forward *port_forward = NULL;

  // a paremeter with 4 parts will have ':' 3 times in it. Hence the +1
  int parts = string_count_char_instances(strIn,':') + 1;
  if (parts < 3 || parts > 4) {
    error("PortForward must have 3 or 4 parts separated by a colon: [bind_address:]local_port:remote_host:remote_port");
    error("The provided paramter \"%s\" has %i parts.",strIn,parts);
    error("Example:  127.0.0.1:123:my.host.com:321  or  123:my.host.com:321"); 
    return NULL;
  }
  trace2("PortForward Parse: From \"%s\" got %i parts:", strIn, parts);

  // get a local copy which we can modify
  local_copy = strPtr = strdup(strIn);
  if (strPtr == NULL) {
    error("error parsing port_forward");
    okay = 0;
  }
  if (okay) {
    port_forward = new_service_port_forward();
  }

  // apply default to optional first part: the local address to bind to
  int index = 0;
  if (okay && parts == 3) {
    index++;
    strncpy(port_forward->srv.bind_address,"127.0.0.1",sizeof(port_forward->srv.bind_address)-1);
    trace2("part %i:  %s  (default)", index,port_forward->srv.bind_address);
  }

  while (okay && (token = strsep(&strPtr,":"))) {
    index++;
    trace2("part %i:  %s", index, token);
    switch(index) {
      case 1:
        strncpy(port_forward->srv.bind_address,token,sizeof(port_forward->srv.bind_address)-1);
        break;
      case 2:
        if (sscanf(token,"%i",&port_forward->srv.port) != 1) {
          error("Error converting \"%s\" to a port number. Input parameter: \"%s\".",token,strIn);
          okay=0;
        }
        break;
      case 3:
        strncpy(port_forward->remote_host,token,sizeof(port_forward->remote_host)-1);
        break;
      case 4:
        if (sscanf(token,"%i",&port_forward->remote_port) != 1) {
          error("Error converting \"%s\" to a port number. Input parameter: \"%s\".",token,strIn);
          okay=0;
        }
        break;
      default: 
        error("error parsing port_forward");
        okay = 0;
        break;
    }
  }

  // clean up after ourselves
  if (local_copy != NULL) {
    free(local_copy);
    local_copy=NULL;
  }


  if (!okay) {
    if (port_forward != NULL) {
      free(port_forward);
      port_forward=NULL;
    }
  }

  return port_forward; 
}

