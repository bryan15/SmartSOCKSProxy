#ifndef SOCKS5_CLIENT_H
#define SOCKS5_CLIENT_H

#include"client_connection.h"
#include"ssh_tunnel.h"

int connect_via_ssh_socks5(client_connection* con, ssh_tunnel* tunnel, int* failure_type);

#endif // SOCKS5_CLIENT_H
