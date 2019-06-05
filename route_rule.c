// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <arpa/inet.h>

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

    rule->have_match_ipv4=0;
    rule->match_ipv4_addr=0;
    rule->match_ipv4_mask=0;

    // host_id_init(&(rule->hid));
    
    rule->tunnel[0]=NULL; 

    rule->file_name[0]=0;
    rule->file_line_number=-1;
  }
  return rule; 
}

route_rule *insert_route_rule(route_rule *head, route_rule *route) {
  trace2("insert_route_rule()");
  if (head == NULL) {
    return route; // the new head!
  }
  route_rule* tmp;
  for (tmp=head; tmp->next != NULL; tmp=tmp->next);
  tmp->next=route;
  route->next = NULL;
  return head;
}

int route_rule_grab_param(char *expected_cmd, char *cmd, char *src, char *dst, int dstlen) {
  if (strcmp(expected_cmd,cmd)==0) {
    strncpy(dst,src,dstlen);
    dst[dstlen-1]=0; // just in case.
    return 1;
  }
  return 0; 
}

int convert_string_to_ipv4_ulong(char *ipaddr_string, unsigned long *ipaddr) {
  struct sockaddr_in sin;
  sin.sin_len=sizeof(sin);
  sin.sin_family=AF_INET;
  if (inet_pton(AF_INET,ipaddr_string,&sin.sin_addr) == 1) {
    *ipaddr=ntohl(sin.sin_addr.s_addr);
    return 1;
  }
  return 0;
}


route_rule *parse_route_rule_spec(char *strIn, char *filename, int line_num, ssh_tunnel *ssh_tunnel_list) {
  char *strPtr;
  char *local_copy;
  char *cmd;
  char *param;

  int okay=1;

  route_rule *route=NULL;
  // get a local copy which we can modify
  local_copy = strPtr = strdup(strIn);
  if (strPtr == NULL) {
    error("error parsing route_rule");
    okay = 0;
  }
  if (okay) {
    route = new_route_rule();
    if (route == NULL) {
      okay=0;
    } 
  }
  if (okay) {
    if (filename == NULL) {
      route->file_name[0]=0;
    } else {
      strncpy(route->file_name,filename,sizeof(route->file_name));
      route->file_name[sizeof(route->file_name)-1]=0;
    }
    route->file_line_number=line_num;
  }

  while(okay && strPtr != NULL) {
    cmd = NULL;
    while(okay && cmd == NULL && strPtr != NULL) {
      cmd = strsep(&strPtr," ");
    }
    if (cmd != NULL && strcmp(cmd,"resolveDNS")==0) {
      route->resolve_dns=1;
      continue;
    }
    if (cmd != NULL && strcmp(cmd,"via")==0) {
      break;
    } 
    param=NULL;
    while(okay && param == NULL && strPtr != NULL) {
      param = strsep(&strPtr," ");
    }
    if (cmd == NULL) {
      break;
    }
    trace2("Got cmd: '%s' param '%s'",cmd,param);
    int got_it = 0; 
    if (!got_it) got_it = route_rule_grab_param("is",cmd,param,route->match_is,sizeof(route->match_is));
    if (!got_it) got_it = route_rule_grab_param("startsWith",cmd,param,route->match_starts_with,sizeof(route->match_starts_with));
    if (!got_it) got_it = route_rule_grab_param("endsWith",cmd,param,route->match_ends_with,sizeof(route->match_ends_with));
    if (!got_it) got_it = route_rule_grab_param("contains",cmd,param,route->match_contains,sizeof(route->match_contains));
    if (!got_it && param != NULL && strcmp(cmd,"port")==0) {
      if (sscanf(param,"%i",&route->match_port) ==1) {
        got_it=1;
      }
    }
    if (!got_it && param != NULL && strcmp(cmd,"network")==0) { 
      // TODO: move parsing of IP address + netmask to a utilty module
      char *ipaddr;
      char *mask;
      ipaddr = strsep(&param,"/");
      mask = strsep(&param,"/");
      // todo: check non-null
      // todo: default mask to /32
      // todo: convert to long unsigned
      if (ipaddr != NULL && convert_string_to_ipv4_ulong(ipaddr, &route->match_ipv4_addr)) {
        got_it=1;
        route->have_match_ipv4=1;
      } 
      if (got_it) {
        if (mask != NULL) {
          // attempt to parse in the form /255.255.0.0
          if (convert_string_to_ipv4_ulong(mask, &route->match_ipv4_mask) == 0) {
            // attempt to parse in the form /24
            int prefix; 
            if (sscanf(mask,"%i",&prefix) == 1) {
              route->match_ipv4_mask = (0xffffffff << (32 - prefix)) & 0xffffffff;
            } else {
              got_it=0; // there's something wrong with this netmask representation
            }
          }
        } else {
          route->match_ipv4_mask=0x0ffffffff;
        }
      }
      trace2("route_rule netmask raw:     %s %s",ipaddr,mask);
      trace2("route_rule netmask parsed:  %i 0x%08lx/0x%08lx",route->have_match_ipv4, route->match_ipv4_addr, route->match_ipv4_mask);
    }
    if (!got_it) {
      okay=0;
    }
  }

  // parse via command
  char *ssh_name;
  while(okay && strPtr != NULL) {
    ssh_name=NULL;
    while(okay && ssh_name == NULL && strPtr != NULL) {
      ssh_name = strsep(&strPtr," ,");
    }
    if (ssh_name != NULL && strlen(ssh_name)>0) {
      ssh_tunnel *ssh;
      for (ssh = ssh_tunnel_list; ssh; ssh=ssh->next) {
        if (strcmp(ssh->name,ssh_name)==0) {
          break;
        }
      }
      if (ssh == NULL) {
        error("attempt to route via an undefined SSH tunnel (%s) in %s line %i: %s", ssh_name, filename, line_num, strIn);
        okay=0;
      } else {
        int i;
        for (i=0; i<(ROUTE_RULE_MAX_SSH_TUNNELS_PER_RULE-1) && route->tunnel[i]!=NULL; i++);
        if (i<ROUTE_RULE_MAX_SSH_TUNNELS_PER_RULE-1) {
          trace2("route via %s",ssh_name);
          route->tunnel[i]=ssh; 
          route->tunnel[i+1]=NULL; 
        } else {
          error("Ignoring excess via \"%s\" (over %i) in %s line %i: %s", ssh_name, ROUTE_RULE_MAX_SSH_TUNNELS_PER_RULE, filename, line_num, strIn);
          // log the error, but otherwise ignore this 
        } 
      }
    }
  }

  // clean up after ourselves
  if (local_copy != NULL) {
    free(local_copy);
    local_copy=NULL;
  }
  if (!okay) {
    if (route != NULL) {
      free(route);
      route=NULL;
    }
  }
  return route;
}

