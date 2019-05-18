#ifndef SERVER_H
#define SERVER_H

#include"proxy_instance.h"
#include"ssh_tunnel.h"

int server(proxy_instance* proxy_instance_list, ssh_tunnel* ssh_tunnel_list);

#endif // SERVER_H
