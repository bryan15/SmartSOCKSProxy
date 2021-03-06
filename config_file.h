// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

#include"log.h"
#include"log_file.h"
#include"proxy_instance.h"
#include"ssh_tunnel.h"
#include"main_config.h"

int config_file_parse(log_file **log_file_list, log_file *log_file_default, 
                      main_config *main_conf,
                      proxy_instance** proxy_instance_list, proxy_instance* proxy_default, 
                      ssh_tunnel** ssh_tunnel_list, ssh_tunnel *ssh_default,
                      char* filename, char** filename_stack, int filename_stack_size, int filename_stack_index);

// exported for unit tests
int read_line(int fd, char* buf, int buflen, int* cr, int* lf, char *filename, int line_num);
int remove_extra_spaces_and_comments_from_config_line(char *inbuf, char *outbuf, int outbuflen);
int parse_line(proxy_instance* proxy_instance_list, ssh_tunnel* ssh_tunnel_list, char *filename, int line_num, char *raw);
int replace_environment_variables_in_string(char *filename, int line_num, char *inbuf, char *outbuf, int outbuflen);

#endif // CONFIG_FILE_H
