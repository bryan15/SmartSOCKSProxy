// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef SSH_TUNNEL_H
#define SSH_TUNNEL_H

#include<time.h>

// TODO: we should support connecting to a SOCKS5 proxy either not managed by this application, or in a remote host. Right now, it *must* be SSH running locally.
// example: 
//   - I want to use an interface different than 127.0.0.1 for the socks proxy Im connecting to
//   - I want to connect to some other host which is already running a socks5 proxy
// in other words, this file must be refactored from "ssh_tunnel" to <something else> which is a super-set
// of ssh_tunnel, direct, null, non-managed local socks proxy, and non-managed remote proxy. 

typedef struct ssh_tunnel {
  struct ssh_tunnel *next;
  unsigned long long id;

  // specified by the user
  char   name[200];            // unique name for this tunnel
  int    socks_port;           // what local port does the SOCKS5 server come up on?
  char   command_to_run[8192]; // how do we build the tunnel?

  // run-time info
  pid_t  pid;
  time_t start_time;
  int    parent_stdin_fd;
  int    parent_stdout_fd;
  int    parent_stderr_fd;
  int    child_stdin_fd;
  int    child_stdout_fd;
  int    child_stderr_fd;

} ssh_tunnel;

// "special" tunnels
extern ssh_tunnel *ssh_tunnel_direct;
extern ssh_tunnel *ssh_tunnel_null;

ssh_tunnel *ssh_tunnel_pool;

ssh_tunnel *new_ssh_tunnel();
ssh_tunnel *insert_ssh_tunnel(ssh_tunnel *head, ssh_tunnel *ssh); 
char *ssh_tunnel_str(ssh_tunnel *ssh, char *buf, int buflen);
ssh_tunnel *parse_ssh_tunnel_spec(char *str);
void reset_ssh_tunnel(ssh_tunnel *ssh);
ssh_tunnel *ssh_tunnel_init(ssh_tunnel *head);

#endif // SSH_TUNNEL_H
