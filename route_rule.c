// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdlib.h>
#include<string.h>

#include"log.h"
#include"route_rule.h"

unsigned long long route_rule_id_pool=0;

route_rule *new_route_rule() {
  route_rule* rule = malloc(sizeof(route_rule));
  if (rule) {
    route_rule_id_pool++;
    rule->id=route_rule_id_pool;
    rule->next=NULL;

    rule->match_is[0]=0;
    rule->match_starts_with[0]=0;
    rule->match_ends_with[0]=0;
    rule->match_contains[0]=0;
    rule->match_port=0;

    rule->resolve_dns=0;

    host_id_init(&(rule->hid));
    
    rule->tunnel[0]=NULL; 

    rule->file_name[0]=0;
    rule->file_line_number=-1;
  }
  return rule; 
}

route_rule *insert_route_rule(route_rule *head, route_rule *rule) {
  trace2("insert_route_rule()");
  rule->next=head;
  return rule; 
}

route_rule *parse_route_rule_spec(char *strIn) {
  char *strPtr;
  char *local_copy;
  char *cmd;
  char *param;

  int okay=1;

  route_rule *rule=NULL;
  // get a local copy which we can modify
  local_copy = strPtr = strdup(strIn);
  if (strPtr == NULL) {
    error("error parsing route_rule");
    okay = 0;
  }
  if (okay) {
    rule = new_route_rule();
    if (rule == NULL) {
      okay=0;
    } 
  }

  while(okay) {
    cmd = NULL;
    while(okay && cmd == NULL && strPtr != NULL) {
      cmd = strsep(&strPtr," ");
    }
    param=NULL;
    while(okay && param == NULL && strPtr != NULL) {
      param = strsep(&strPtr," ");
    }
    trace("Got cmd: '%s' param '%s'\n");
  }

  // clean up after ourselves
  if (local_copy != NULL) {
    free(local_copy);
    local_copy=NULL;
  }
  if (!okay) {
    if (rule != NULL) {
      free(rule);
      rule=NULL;
    }
  }
  return rule;
}

