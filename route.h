#ifndef ROUTE_H
#define ROUTE_H

typedef struct route {
  unsigned long long id;
  struct route *next;
  char name[200];
  char ssh_command[8192];
} route;

#endif // ROUTE_H

