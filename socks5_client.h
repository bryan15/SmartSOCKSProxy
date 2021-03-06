// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef SOCKS5_CLIENT_H
#define SOCKS5_CLIENT_H

#include"client_connection.h"
#include"ssh_tunnel.h"

int connect_via_ssh_socks5(client_connection* con, ssh_tunnel *tun, int* failure_type);

#endif // SOCKS5_CLIENT_H
