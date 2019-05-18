#ifndef SERVICE_SOCKS_H
#define SERVICE_SOCKS_H

#include"service.h"

#define SOCKS_VERSION_ERROR (-1)
#define SOCKS_AUTH_ERROR    (-1)

typedef struct service_socks {
  struct service srv;
} service_socks;

service_socks *new_service_socks();
service_socks *parse_service_socks_spec(char *str);

#endif // SERVICE_SOCKS_H
