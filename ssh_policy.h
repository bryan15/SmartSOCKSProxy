#ifndef SSH_POLICY_H
#define SSH_POLICY_H

#include"proxy_instance.h"
#include"ssh_tunnel.h"

void check_ssh_tunnels(proxy_instance *proxy_instance_list, ssh_tunnel *ssh_tunnel_list);

#endif // SSH_POLICY_H
