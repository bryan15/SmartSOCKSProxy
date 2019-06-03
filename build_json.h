// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef BUILD_JSON_H
#define BUILD_JSON_H

#include"proxy_instance.h"

char *build_json(proxy_instance *proxy_instance_list, time_t proxy_start_time, ssh_tunnel *ssh_tunnel_list);

#endif // BUILD_JSON_H
