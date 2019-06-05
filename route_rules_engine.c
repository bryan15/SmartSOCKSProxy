// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<string.h>
#include <arpa/inet.h>

#include"proxy_instance.h"
#include"service.h"
#include"client_connection.h"
#include"route_rule.h"
#include"ssh_tunnel.h"
#include"string2.h"
#include"dns_util.h"

route_rule *default_direct = NULL;

char *rre_get_host_id_name(host_id *dst, char *buf, int buflen) {
  if (host_id_has_name(dst)) {
    return host_id_get_name(dst);
  }
  return NULL;
}

// Improvement: support IPv6
char *rre_get_host_id_addr(host_id *dst, char *buf, int buflen, sa_family_t *family, unsigned long *ipv4_addr) {
  if (host_id_has_addr(dst)) {
    host_id_addr_str(dst, buf, buflen);
    *family=(host_id_get_addr(dst)->sa_family);
    if (*family == AF_INET) {
      *ipv4_addr = ntohl(((struct sockaddr_in*)host_id_get_addr(dst))->sin_addr.s_addr);
    } else {
      *ipv4_addr=0;
    }
    return buf;
  }
  *family = AF_UNSPEC;
  *ipv4_addr = 0;
  return NULL; 
}


int decide_applicable_rule(proxy_instance *proxy, service *srv, client_connection *con) {
  host_id *dst;
  char name_mem[MAX_DNS], *name;
  char ipaddr_mem[MAX_DNS], *ipaddr;
  unsigned long ipv4_addr;
  int port;
  sa_family_t family; 

  dst = &con->dst_host;
  name = rre_get_host_id_name(dst, name_mem, sizeof(name_mem));
  ipaddr = rre_get_host_id_addr(dst,ipaddr_mem, sizeof(ipaddr_mem), &family, &ipv4_addr);
  port = host_id_get_port(dst);
 
  route_rule *applicable_route=NULL;
  for (route_rule *route = proxy->route_rule_list; route && !applicable_route; route=route->next) {
    int this_route_applies=1;
    int final_route=0;
     
    if (this_route_applies && route->match_is[0] != 0) {
      this_route_applies=0;
      if (name   &&  strcmp(name,   route->match_is) == 0)   { this_route_applies=1; }
      if (ipaddr &&  strcmp(ipaddr, route->match_is) == 0)   { this_route_applies=1; }
      if (this_route_applies) {
        debug("%s line %i: host %s(%s) matches %s",route->file_name, route->file_line_number, name, ipaddr, route->match_is);
      } else {
        debug("%s line %i: host %s(%s) DOES NOT match %s",route->file_name, route->file_line_number, name, ipaddr, route->match_is);
      }
    }
    if (this_route_applies && route->match_starts_with[0] != 0) {
      this_route_applies=0;
      if (name   && string_starts_with(name,   route->match_starts_with))  { this_route_applies = 1; }
      if (ipaddr && string_starts_with(ipaddr, route->match_starts_with))  { this_route_applies = 1; }
      if (this_route_applies) {
        debug("%s line %i: host %s(%s) starts with %s",route->file_name, route->file_line_number, name, ipaddr, route->match_starts_with);
      } else {
        debug("%s line %i: host %s(%s) DOES NOT start with %s",route->file_name, route->file_line_number, name, ipaddr, route->match_starts_with);
      }
    }
    if (this_route_applies && route->match_ends_with[0] != 0) {
      this_route_applies=0;
      if (name   && string_ends_with(name,   route->match_ends_with))  { this_route_applies = 1; }
      if (ipaddr && string_ends_with(ipaddr, route->match_ends_with))  { this_route_applies = 1; }
      if (this_route_applies) {
        debug("%s line %i: host %s(%s) ends with %s",route->file_name, route->file_line_number, name, ipaddr, route->match_ends_with);
      } else {
        debug("%s line %i: host %s(%s) DOES NOT end with %s",route->file_name, route->file_line_number, name, ipaddr, route->match_ends_with);
      }
    }
    if (this_route_applies && route->match_contains[0] != 0) {
      this_route_applies=0;
      if (name   && string_contains(name,   route->match_contains))  { this_route_applies = 1; }
      if (ipaddr && string_contains(ipaddr, route->match_contains))  { this_route_applies = 1; }
      if (this_route_applies) {
        debug("%s line %i: host %s(%s) contains %s",route->file_name, route->file_line_number, name, ipaddr, route->match_contains);
      } else {
        debug("%s line %i: host %s(%s) DOES NOT contain %s",route->file_name, route->file_line_number, name, ipaddr, route->match_contains);
      }
    }
    if (this_route_applies && route->match_port != 0) {
      if (route->match_port != port) {
        this_route_applies=0;
      }
      if (this_route_applies) {
        debug("%s line %i: port %i matches %i",route->file_name, route->file_line_number, port, route->match_port);
      } else {
        debug("%s line %i: port %i DOES NOT match %i",route->file_name, route->file_line_number, port, route->match_port);
      }
    }
    if (this_route_applies && route->have_match_ipv4) {
      this_route_applies=0;
      if (ipaddr && family == AF_INET) {
        unsigned long dst_net  = ipv4_addr & route->match_ipv4_mask;
        unsigned long rule_net = route->match_ipv4_addr & route->match_ipv4_mask;
        if (dst_net == rule_net) { this_route_applies = 1; }
      } 
      if (this_route_applies) {
        debug("%s line %i: host %s(%s) matches network 0x08x / 0x08x",route->file_name, route->file_line_number, name, ipaddr, route->match_ipv4_addr);
      } else {
        debug("%s line %i: host %s(%s) DOES NOT match network 0x08x / 0x08x",route->file_name, route->file_line_number, name, ipaddr, route->match_ipv4_addr);
      }
    }
   
    if (route->resolve_dns) {
      debug("%s line %i: Executing DNS lookup for %s",route->file_name, route->file_line_number, name);
      resolve_dns_for_host_id(dst);
      name = rre_get_host_id_name(dst, name_mem, sizeof(name_mem));
      ipaddr = rre_get_host_id_addr(dst,ipaddr_mem, sizeof(ipaddr_mem), &family, &ipv4_addr);
    }

    if (this_route_applies && route->tunnel[0] != NULL) {
      debug("%s line %i: this route rule applies and has a tunnel specified. I will stop here - this is the matching rule.",route->file_name, route->file_line_number, name);
      final_route = 1;
    }
 
    if (this_route_applies && final_route) {
      applicable_route=route;
    }
  }

  if (!default_direct) {
    default_direct=new_route_rule();
    strcpy(default_direct->file_name,"default_direct");
    default_direct->tunnel[0]=ssh_tunnel_direct;
    default_direct->tunnel[1]=NULL;
  }

  int returnval = 1;
  if (!applicable_route) {
    debug("No matching route_rule found for %s (%s) port %i. Using default.", name, ipaddr, port);
    returnval = 0;
    applicable_route = default_direct;
  }

  lock_client_connection(con);
  con->route = applicable_route;
  unlock_client_connection(con);
  return returnval;
}



