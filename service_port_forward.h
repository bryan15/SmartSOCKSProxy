#ifndef SERVICE_PORT_FORWARD_H 
#define SERVICE_PORT_FORWARD_H

#include"service.h"

typedef struct service_port_forward {
  struct service srv;

  char remote_host[300];
  int remote_port;
} service_port_forward;

service_port_forward *new_port_forward();
service_port_forward *parse_service_port_forward_spec(char *str);

#endif // SERVICE_PORT_FORWARD_H

