// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef SERVER_H
#define SERVER_H

#include"log_file.h"
#include"proxy_instance.h"
#include"ssh_tunnel.h"

int server(log_file* log_file_list, proxy_instance* proxy_instance_list, ssh_tunnel* ssh_tunnel_list, log_config *main_log_config);

#endif // SERVER_H
