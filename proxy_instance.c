// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#include"log.h"
#include"proxy_instance.h"

proxy_instance *new_proxy_instance() {
  proxy_instance *pinst;

  pinst=malloc(sizeof(proxy_instance));
  if (pinst == NULL) {
    unexpected_exit(50,"Error allocating new proxy_instance"); 
  }

  pinst->next=NULL;
  pinst->name[0]='x';
  pinst->name[1]=0;

  pinst->service_list=NULL;
  pinst->route_rule_list=NULL;

  pinst->client_connection_list=NULL;

  log_config_init(&(pinst->log));
  pinst->log.level=LOG_LEVEL_ERROR;
    
  return pinst;
}

proxy_instance *insert_proxy_instance(proxy_instance *head, proxy_instance *pinst) {
  // I don't expect to ever destroy a proxy instance, so no need for any fancy list structure
  // Although... I'm lazy in javascript, so I append to the end so JSON ordering matches the config file.
  if (head == NULL) {
    return pinst; // the new head!
  } 
  proxy_instance *pp;
  for (pp=head; pp->next != NULL; pp=pp->next); 
  pp->next=pinst;
  pinst->next = NULL; 
  return head;
}

proxy_instance *new_proxy_instance_from_template(proxy_instance *template) {
  proxy_instance* pinst = new_proxy_instance();
  if (pinst == NULL || template == NULL) {
    return pinst;
  }

  pinst->log.level = template->log.level;  
  pinst->log.file  = template->log.file;  

  return pinst;
}

char *proxy_instance_str(proxy_instance *pinst, char *buf, int buflen) {
  if (buf) {
    buf[0]=0;
  }
  if (buf && pinst) {
    if (pinst->log.file) {
      snprintf(buf, buflen, "%s log verbosity %s to %s",pinst->name, log_level_str(pinst->log.level), pinst->log.file->file_name);
    } else {
      snprintf(buf, buflen, "%s log verbosity %s to STDOUT",pinst->name, log_level_str(pinst->log.level));
    }
  }
  return buf;
}


