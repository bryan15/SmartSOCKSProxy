// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<sys/wait.h>
#include<fcntl.h>

#include"log.h"
#include"ssh_tunnel.h"
#include"proxy_instance.h"
#include"client_connection.h"

void ssh_tunnel_close_pipe(int fd) {
  int rc;
  if (fd < 0) {
    return;
  }
  do {
    close(fd);
  } while (rc<0 && errno==EINTR);
  if (rc<0) {
    errorNum("close()");
  }
}

void ssh_tunnel_close_pipes(ssh_tunnel *ssh) {
  ssh_tunnel_close_pipe(ssh->parent_stdin_fd);
  ssh_tunnel_close_pipe(ssh->parent_stdout_fd);
  ssh_tunnel_close_pipe(ssh->parent_stderr_fd);
  ssh_tunnel_close_pipe(ssh->child_stdin_fd);
  ssh_tunnel_close_pipe(ssh->child_stdout_fd);
  ssh_tunnel_close_pipe(ssh->child_stderr_fd);
  ssh->parent_stdin_fd=-1;
  ssh->parent_stdout_fd=-1;
  ssh->parent_stderr_fd=-1;
  ssh->child_stdin_fd=-1;
  ssh->child_stdout_fd=-1;
  ssh->child_stderr_fd=-1;
}

void ssh_tunnel_set_nonblocking(int fd) {
  int flags;
  int rc;
  do {
    flags  = fcntl(fd,F_GETFD);
  } while (flags < 0 && errno == EINTR);
  flags |= O_NONBLOCK;
  if (flags < 0) {
    errorNum("fcntl()");
    unexpected_exit(86,"fcntl()");
  }
  do {
    rc=fcntl(fd, F_SETFD, flags);
  } while (rc < 0 && errno == EINTR); 
  if (rc < 0) {
    errorNum("fcntl()");
    unexpected_exit(87,"fcntl()");
  }
}

int create_tunnel_pipes(ssh_tunnel *ssh) {
  ssh_tunnel_close_pipes(ssh);

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

  ssh_tunnel_set_nonblocking(ssh->parent_stdout_fd);
  ssh_tunnel_set_nonblocking(ssh->parent_stderr_fd);

  return 1; 
}


int ssh_report_interval=10;
int check_ssh_tunnel(ssh_tunnel *ssh) {
  int is_running=0;
  int needs_to_run=0;
  time_t cur_time = time(NULL);

  int should_be_running = 0;
  if (ssh->mark > 0) {
    should_be_running=1;
  }


  // check if a pre-existing process has exited. 
  int did_update=0;
  if (ssh->pid > 0) {
    int exit_code;
    pid_t tmp;
    do {
      tmp=waitpid(ssh->pid,&exit_code,WNOHANG);
    } while (tmp<0 && errno==EINTR);
    if (tmp == 0) {
      if (cur_time - ssh->last_update_time >= ssh_report_interval) {
        trace("SSH child for %s (%llu) pid %i still running... mark=%i connection_count=%i", ssh->name, ssh->id, ssh->pid,ssh->mark,ssh->connection_count);
        ssh->last_update_time = cur_time;
      }
      is_running=1;
    } else if (tmp > 0) {
      debug("SSH child for %s (%llu) pid %i exited, code = %i",ssh->name, ssh->id, ssh->pid, exit_code);
      ssh_tunnel_close_pipes(ssh);
      ssh->pid = -1;
    } else {
      // should never happen. 
      error("waitpid() for SSH child for %s (%llu) pid %i. This should never happen.",ssh->name, ssh->id, ssh->pid);
    }
  } 

  if (should_be_running) {
    needs_to_run=1;
  }

  if (!is_running && needs_to_run) {
    if (time(NULL) - ssh->start_time < 2) { // "poor-programmer"'s rate throttling
      needs_to_run=0;
    }
  }

  if (!is_running && needs_to_run) {
    // start SSH tunnel
    create_tunnel_pipes(ssh);
    ssh->pid = fork();
    if (ssh->pid > 0) {
      ssh->start_time = time(NULL);
      debug("SSH child started for %s (%i) pid %i: %s", ssh->name, ssh->id, ssh->pid, ssh->command_to_run);
    } else if (ssh->pid == 0) {
      int rc;
      // re-route STDOUT and STDERR
      rc=dup2(ssh->child_stdin_fd, STDIN_FILENO);
      if (rc < 0) {
        errorNum("dup2()");
        unexpected_exit(81,"dup2()");
      }
      rc=dup2(ssh->child_stdout_fd, STDOUT_FILENO);
      if (rc < 0) {
        errorNum("dup2()");
        unexpected_exit(82,"dup2()");
      }
      rc=dup2(ssh->child_stderr_fd, STDERR_FILENO);
      if (rc < 0) {
        errorNum("dup2()");
        unexpected_exit(83,"dup2()");
      }
      char cmd_buf[8192];
      snprintf(cmd_buf,sizeof(cmd_buf),"exec %s",ssh->command_to_run);
      rc=execlp( "/bin/bash", "bash", "-c", cmd_buf, NULL);
      errorNum("execlp() returned %i",rc);
      unexpected_exit(84,"execlp()");
    } else {
      errorNum("fork()");
    }
  } 
  return did_update;
}


void check_ssh_tunnels(proxy_instance *proxy_instance_list, ssh_tunnel *ssh_tunnel_list) {
  proxy_instance *proxy;
  client_connection *con;
  ssh_tunnel *ssh; 
  
  // We mark-and-sweep to identify and activate required SSH tunnels
  for (ssh=ssh_tunnel_list; ssh; ssh = ssh->next) {
    ssh->mark=0;
    ssh->connection_count=0;
  }
  for (proxy=proxy_instance_list; proxy; proxy = proxy->next) {
    for (con=proxy->client_connection_list; con; con=con->next) {
      route_rule *route_in_use;
      ssh_tunnel *tunnel_in_use;
      lock_client_connection(con);
      route_in_use  = con->route;
      tunnel_in_use = con->tunnel;
      unlock_client_connection(con);

      if (route_in_use != NULL) {
        if (tunnel_in_use) {
          tunnel_in_use->mark++;
          tunnel_in_use->connection_count++;
        } else { 
          // This connection has a route decided, but has failed to successfully connect to a required tunnel. 
          // Let's look at all of this route's tunnels and mark them as required. 
          ssh_tunnel **list = route_in_use->tunnel;
          for (int i=0; list[i]; i++) {
            list[i]->mark++;
          }
        }
      }
    }
  } 

  // Now go update tunnel status and check any that should be running. 
  for (ssh=ssh_tunnel_list; ssh; ssh = ssh->next) {
    if (ssh == ssh_tunnel_direct || ssh == ssh_tunnel_null) {
      continue;
    }
    check_ssh_tunnel(ssh);
  }
}

