// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

#include"proxy_instance.h"
#include"ssh_tunnel.h"

int config_file_parse(log_file **log_file_list, log_file *log_file_default, proxy_instance** proxy_instance_list, proxy_instance* proxy_default, ssh_tunnel** ssh_tunnel_list, ssh_tunnel *ssh_default, char* filename);

// exported for unit tests
int read_line(int fd, char* buf, int buflen, int* cr, int* lf, char *filename, int line_num);
int remove_extra_spaces_and_comments_from_config_line(char *inbuf, char *outbuf, int outbuflen);
int parse_line(proxy_instance* proxy_instance_list, ssh_tunnel* ssh_tunnel_list, char *filename, int line_num, char *raw);

#endif // CONFIG_FILE_H
