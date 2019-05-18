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

  // "resolveDns" command
  int resolve_dns; // boolean

  // "network" command to match according to IPv4 netmask
  host_id hid;

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
route_rule *parse_route_rule_spec(char *strIn);

#endif // ROUTE_RULE_H
