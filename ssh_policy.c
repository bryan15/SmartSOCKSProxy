// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<sys/wait.h>
#include<unistd.h>
/*
#include<stdlib.h>
#include<string.h>
#include<pthread.h>
#include<netdb.h>
#include<netinet/in.h>
#include<sys/select.h>
#include<errno.h>
#include<stdio.h>
*/

#include"log.h"
#include"ssh_tunnel.h"
#include"proxy_instance.h"
#include"client_connection.h"

#define SSH_TUNNEL_VICTORIA 1
#define SSH_TUNNEL_CALGARY 2

int start_ssh_tunnel(ssh_tunnel *ssh) {
  // 0 = read end, 1 = write end
  int pipe_stdin[2];
  int pipe_stdout[2];
  int pipe_stderr[2];
  if (pipe(pipe_stdin) < 0 ||
      pipe(pipe_stdout) < 0 ||
      pipe(pipe_stderr) < 0) {
    errorNum("pipe()");
    unexpected_exit(85,"pipe()");
  }
  ssh->parent_stdin_fd = pipe_stdin[1];
  ssh->child_stdin_fd = pipe_stdin[0];
  ssh->parent_stdout_fd = pipe_stdout[0];
  ssh->child_stdout_fd = pipe_stdout[1];
  ssh->parent_stderr_fd = pipe_stderr[0];
  ssh->child_stderr_fd = pipe_stderr[1];
 
  return 1; 
}

void check_ssh_tunnels(proxy_instance *proxy_instance_list, ssh_tunnel *ssh_tunnel_list) {
}

time_t time_last_report=0;
int ssh_report_interval=10;
void check_ssh_tunnel(int child_stdout, client_connection *pool, pid_t *pid, time_t *last_start, int route, int always_run_this_tunnel) {
  int is_running=0;
  int needs_to_run=0;
  time_t cur_time = time(NULL);

  int tunnel_id=1;

  // check if a pre-existing process has exited. 
  if (*pid > 0) {
    int exit_code;
    pid_t tmp=waitpid(*pid,&exit_code,WNOHANG);
    if (tmp == 0) {
      if (cur_time - time_last_report >= ssh_report_interval) {
        trace("SSH child for %i pid %i still running...",tunnel_id, *pid);
        time_last_report = cur_time;
      }
      is_running=1;
    } else if (tmp > 0) {
      debug("SSH child for %i pid %i exited, code = %i",tunnel_id, *pid, exit_code);
      *pid = 0;
    } else {
      error("waitpid() for SSH child for %i pid %i",tunnel_id, *pid);
    }
  } 

  // check if we need a tunnel

  if (always_run_this_tunnel) {
    needs_to_run=1;
  } else {
    client_connection *con;
    for(con=pool; con; con=con->next) {
      lock_client_connection(con);
      // FIXME FIXME FIXME TODO FIXME ROUTING SHIT GOES HERE
      //if (con->route == route) needs_to_run=1;
      unlock_client_connection(con);
    }
  }

  if (!is_running && needs_to_run) {
    if (time(NULL) - *last_start < 2) { // "poor-programmer"'s rate throttling
      needs_to_run=0;
    }
  }

  if (!is_running && needs_to_run) {
    // start SSH tunnel
    *pid = fork();
    if (*pid > 0) {
      *last_start = time(NULL);
      debug("SSH child started for %i pid %i",tunnel_id, *pid);
    } else if (*pid == 0) {
      int rc;
      // re-route STDOUT and STDERR
      rc=dup2(child_stdout, STDOUT_FILENO);
      if (rc < 0) {
        errorNum("dup2()");
        unexpected_exit(82,"dup2()");
      }
      rc=dup2(child_stdout, STDERR_FILENO);
      if (rc < 0) {
        errorNum("dup2()");
        unexpected_exit(83,"dup2()");
      }
      // execlp() should never return... if it does, it's an error.
      char *hostname="bastion1.example.com";
      char *port="127.0.0.1:31000";  // thou shalt never EVER, for security reasons, bind to any interface other than local loopback
      // TODO SSH tunnel management needs rework
      /*
      if (tunnel_id == SSH_TUNNEL_ZONE2) {
        hostname="bastion2.example.com";
        port="127.0.0.1:31001"; // thou shalt never EVER, for security reasons, bind to any interface other than local loopback
      } else if (tunnel_id == SSH_TUNNEL_ZONE3) {
        hostname="bastion3.example.com";
        port="127.0.0.1:31002"; // thou shalt never EVER, for security reasons, bind to any interface other than local loopback
      }
      */
      //rc=execlp( "/usr/bin/ssh", "ssh", "-D", port, "-F", "/dev/null", "-N", "-o", "ServerAliveInterval=5", hostname, NULL);
      rc=execlp( "/usr/local/bin/ssh", "ssh", "-D", port, "-N", "-o", "ServerAliveInterval=5", hostname, NULL);
      errorNum("execlp() returned %i",rc);
      unexpected_exit(84,"execlp()");
    } else {
      errorNum("fork()");
    }
  }
}

