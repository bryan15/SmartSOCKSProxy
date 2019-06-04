// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef ROUTE_RULE_H
#define ROUTE_RULE_H

#include"host_id.h"
#include"ssh_tunnel.h"

#define ROUTE_RULE_MAX_SSH_TUNNELS_PER_RULE 100

typedef struct route_rule {
  struct route_rule *next;
  unsigned long long id;

  // match against DNS or string-verison of hid
  char match_is[MAX_DNS];
  char match_starts_with[MAX_DNS];
  char match_ends_with[MAX_DNS];
  char match_contains[MAX_DNS];
  int  match_port;

  int have_match_ipv4; // boolean, because both values below could legitimately be 0
  unsigned long match_ipv4_addr;
  unsigned long match_ipv4_mask;

  // "resolveDns" command
  int resolve_dns; // boolean

  // "via" command which assigns a route if conditions are true
  ssh_tunnel* tunnel[ROUTE_RULE_MAX_SSH_TUNNELS_PER_RULE];

  // unrelated to rule or its operation, but for debugging and
  // producing useful log messages, let's
  // remember where this rule came from.
  char file_name[4096];
  long file_line_number;

  // metrics
  unsigned long long num_matches;
} route_rule;

route_rule *new_route_rule();
route_rule *insert_route_rule(route_rule *head, route_rule *rule);
route_rule *parse_route_rule_spec(char *strIn, char *filename, int line_num, ssh_tunnel *ssh_tunnel_list);

#endif // ROUTE_RULE_H
