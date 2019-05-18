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
  pinst->client_connection_list=NULL;

  // TODO: these should be inherited from the default instance
  pinst->log_level=LOG_ERROR;
    
  return pinst;
}

proxy_instance *insert_proxy_instance(proxy_instance *head, proxy_instance *pinst) {
  // I don't expect to ever destroy a proxy instance, so no need for any fancy list structure
  if (pinst) {
    pinst->next = head;
  }
  return pinst; // pinst is the new head
}

proxy_instance *new_proxy_instance_from_template(proxy_instance *template) {
  proxy_instance* pinst = new_proxy_instance();
  if (pinst == NULL || template == NULL) {
    return pinst;
  }

  pinst->log_level = template->log_level;  
  pinst->log_max_size = template->log_max_size;  
  pinst->log_max_rotate = template->log_max_rotate;  
  // what happens if multiple instances use the same log filename? I think it'll be okay because
  // all logfile operations are synchronous.
  strncpy(pinst->log_file_name, template->log_file_name, LOG_LOGFILE_NAME_MAX_LEN-1);  
  pinst->log_file_name[LOG_LOGFILE_NAME_MAX_LEN-1]=0;

  return pinst;
}

char *proxy_instance_str(proxy_instance *pinst, char *buf, int buflen) {
  if (buf) {
    buf[0]=0;
  }
  if (buf && pinst) {
    snprintf(buf, buflen, "%s verbosity %i",pinst->name, pinst->log_level);
  }
  return buf;
}


