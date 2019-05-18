// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef SOCKS_CONNECTION_H
#define SOCKS_CONNECTION_H

#include"proxy_instance.h"
#include"service.h"
#include"client_connection.h"

int socks_connect(proxy_instance *proxy, service *srv, client_connection *con, int *failure_type);
int socks_connect_shuttle(client_connection *con);
char *socks_connect_str_destination(client_connection *con, char *buf, int buflen);

#endif // SOCKS_CONNECTION_H
