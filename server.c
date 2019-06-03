// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include<sys/select.h>
#include<sys/socket.h>
#include<sys/signal.h>
#include<errno.h>
#include<signal.h>

#include"log.h"
#include"log_file.h"
#include"client_connection.h"
#include"ssh_tunnel.h"
#include"ssh_policy.h"
#include"listen_socket.h"
#include"proxy_instance.h"
#include"thread_local.h"
#include"build_json.h"
#include"thread_msg.h"

int exit_server=0;

#define CLOSED_CONNECTION_LOITER_TIME_S  4 // time in seconds closed connection objects will remain in the datastructure before being cleaned up

// IMPROVEMENT: receive power notifications. See https://developer.apple.com/library/content/qa/qa1340/_index.html

void signal_callback_handler(int signum) {
  printf("Caught signal %d\n",signum);
}

// Create the thread using POSIX routines.
void launch_thread(proxy_instance *proxy, service *srv, client_connection *con) {
  pthread_attr_t  attr;
  int             rc;

  // create a temporary DTO
  thread_data *data = malloc(sizeof(thread_data));
  if (!data) {
    unexpected_exit(80,"cannot allocate thread_data");
  }
  data->proxy = proxy;
  data->srv = srv;
  data->con = con;
 
  rc = pthread_attr_init(&attr);
  if (rc != 0) {
    errno=rc;
    errorNum("pthread_attr_init()");
    con->thread_has_exited=1;
    return;
  }

  rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED); // don't need to call thread_join()
  //rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);   // need to call thread_join()
  if (rc != 0) {
    errno=rc;
    errorNum("pthread_attr_setdetachstate()");
    con->thread_has_exited=1;
    return;
  }

  // this is where the thread is created 
  // After this point, and until the thread ends, use of mutex for values which change is required. See: 
  //   lock_client_connection()
  //   unlock_client_connection()
  con->pthread_create_value =  pthread_create(&(con->thread_id), &attr, srv->connection_handler, data);

  trace("pthread_create() = %i",con->pthread_create_value);
  con->pthread_create_called=1;
  if (con->pthread_create_value != 0) {
    errno=con->pthread_create_value;
    errorNum("pthread_create()");
    con->thread_has_exited=1;
  } else {
    trace("thread created");
  }
 
  rc = pthread_attr_destroy(&attr);
  if (rc != 0) {
    errno=rc;
    errorNum("pthread_attr_destroy()");
  }
}

client_connection *accept_connection(service *srv) {
  struct sockaddr_in new_client_addr;
  char tmpbuf[100];
  socklen_t len=sizeof(new_client_addr);
  int new_client_fd;
  do {
    new_client_fd = accept(srv->fd,(struct sockaddr *)&new_client_addr, &len);
  } while (new_client_fd < 0 && (errno==EINTR || errno==EAGAIN));

  if (new_client_fd <0) { 
    errorNum("accept()");
  } else {
    // looks good; lets setup a new client_connection
    client_connection *con = new_client_connection();
    con->srv=(void*)srv;
    con->fd_in=new_client_fd;
    host_id_set_addr_in(&(con->src_host),&new_client_addr);
    return con;
  }
  return NULL;
}

void dump_pool(client_connection *pool) {
  client_connection *cur, *next;

  if (pool == NULL) { 
    trace(">>-- pool null");
  } else {
    cur=pool; 
    next=cur->next;
    trace(">>-- pool:");
    while(cur != NULL) {
      trace("        %llu",cur->id);
      cur=next;
      if (cur != NULL) {
        next=cur->next;
      }
    }
  } 
}

client_connection *cleanup_connections(client_connection *pool) {
  client_connection *cur, *next; 
  time_t now = time(NULL);
  if (pool != NULL) { 
    cur=pool; 
    next=cur->next;
    while(cur != NULL) {
      if (cur->thread_has_exited && (now - cur->end_time > CLOSED_CONNECTION_LOITER_TIME_S )) {
        if (pool == cur) {
          pool=next;
        }
        free_client_connection(cur);
      } 
      cur=next;
      if (cur != NULL) {
        next=cur->next;
      }
    }
  }      
  return pool;
}

int set_fds(int fd, int fd_max, fd_set *set1, fd_set *set2, fd_set *set3) {
  if (fd <0) {
    return fd_max;
  }
  if (set1) { FD_SET(fd,set1); }
  if (set2) { FD_SET(fd,set2); }
  if (set3) { FD_SET(fd,set3); }
  if (fd > fd_max) {
    return fd;
  }
  return fd_max; 
}

void read_from_child(char *label, ssh_tunnel *ssh, int fd) {
  int rc; 
  char tmpbuf[2000]; 
  rc=read(fd,tmpbuf,sizeof(tmpbuf)-1);
  if (rc < 0) {
    errorNum("read(child stdout/stderr)");
    unexpected_exit(91,"read()");
  }
  if (rc > 0) {
    tmpbuf[rc]=0;
    // special convenience case... lop off trailing \n
    if (rc > 0 && tmpbuf[rc-1] == '\n') {
      tmpbuf[rc-1]=0;
    }
    info("CHILD %S: %s",label,tmpbuf);
  }
}

// Main loop
//int server(int port, port_forward *port_forward_pool) {
int server(log_file *log_file_list, proxy_instance *proxy_instance_list, ssh_tunnel *ssh_tunnel_list, log_config *main_log_config) {

  // On a Macbook coming out of sleep, seems we get a SIGPIPE on our sockets & pipes. 
  // Example error message:
  //   INFO   9471: Routed connection 9470 127.0.0.1:59196 -> CONNECT via direct 35.161.16.31:443
  //   INFO   9470: Routed connection 9471 127.0.0.1:59197 -> CONNECT via direct 35.161.16.31:443
  //   ERROR:  Caught signal 13
  //   ERROR:  Error on listen socket. Exiting. in server.c line 262
  //   write(): Broken pipe in safe_blocking_readwrite.c line 81
  //   ERROR  9468: write(): Broken pipe in shuttle.c line 28
  // Problem is: the signal kills the application. 
  // So we just ignore it; we disable it on the socket. 
  // See https://stackoverflow.com/questions/108183/how-to-prevent-sigpipes-or-handle-them-properly
  // Another option is to handle the signal in a callback. But, we're not really setup to handle signals. 
  // Improvement: Use SIGPIPE handler to set a flag and check it at appropriate points in time.
  // 
  //if (signal(SIGPIPE,signal_callback_handler) == SIG_ERR) {
  //  unexpected_exit(92,"signal()");
  //}
  if (signal(SIGPIPE,SIG_IGN) == SIG_ERR) {
    unexpected_exit(92,"signal()");
  }

  // We have limited need for worker threads to wake-up the main thread 
  // (ie: break out of blocking select())
  // For this purpose, we create a pipe. This pipe is included
  // in select(). To wake up the main thread, write a byte to the
  // pipe. That's it. Currently, the contents of the data written
  // to the pipe is irrelevant and discarded. 
  int msg_pipe[2];
  if (pipe(msg_pipe) < 0) {
    errorNum("pipe()");
    unexpected_exit(86,"pipe()");
  }
  thread_msg_init(msg_pipe[1]);
  int thread_msg_fd = msg_pipe[0];

  // initialize proxy instances & all listening services
  thread_local_set_log_config(NULL);
  for (proxy_instance *proxy = proxy_instance_list; proxy; proxy = proxy->next) {
    thread_local_set_proxy_instance(proxy); // for any log output generated during this initialization work
    for (service *srv = proxy->service_list; srv; srv=srv->next) {
      thread_local_set_service(srv);
      srv->fd = listen_socket(srv->bind_address, srv->port);
    }
  } 
  thread_local_set_service(NULL);
  thread_local_set_proxy_instance(NULL);
  thread_local_set_log_config(main_log_config);

  time_t proxy_start_time = time(NULL);

  struct sockaddr_in client_addr;
  fd_set readfds, writefds, errorfds;
  struct timeval timeout;
  int maxfd=-1;
  int rc;
  client_connection *pool=NULL;
  while (!exit_server) { 
    //trace2("loop"); // some things are too much even for trace2

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&errorfds);

    for (proxy_instance *proxy=proxy_instance_list; proxy; proxy = proxy -> next) {
      for (service *srv = proxy->service_list; srv; srv=srv->next) {
        maxfd = set_fds(srv->fd, maxfd, &readfds, &errorfds, NULL);
      }
    }

    for (ssh_tunnel *ssh=ssh_tunnel_list; ssh; ssh=ssh->next) {
      maxfd = set_fds(ssh->parent_stdout_fd, maxfd, &readfds, &errorfds, NULL);
      maxfd = set_fds(ssh->parent_stderr_fd, maxfd, &readfds, &errorfds, NULL);
    }
    maxfd = set_fds(thread_msg_fd, maxfd, &readfds, &errorfds, NULL);

    // this is arbitrary... no magic here. 
    // just wake up periodically and reap dead threads.
    timeout.tv_sec=1;
    timeout.tv_usec=200000;

    //trace("select()...");
    do {
      rc=select(maxfd+1, &readfds,&writefds,&errorfds,&timeout);
    } while (errno == EAGAIN);
    //trace("... select() returned %i",rc);
    if (rc <0 && errno != EINTR) {
      errorNum("listen select()");
      unexpected_exit(87,"select()");
    }

    thread_local_set_log_config(NULL);
    for (proxy_instance *proxy=proxy_instance_list; proxy; proxy = proxy -> next) {
      thread_local_set_proxy_instance(proxy); // for log messages
      for (service *srv = proxy->service_list; srv; srv=srv->next) {
        thread_local_set_service(srv); // for log messages
        if (FD_ISSET(srv->fd,&errorfds)) {
          error("Error on listen socket. Exiting.");
          unexpected_exit(88,"service fd error"); // FIXME: should handle this more elegantly. Maybe no need to exit. But it should never happen if we properly handle all events. 
        }
      }
    }
    thread_local_set_service(NULL);
    thread_local_set_proxy_instance(NULL);
    thread_local_set_log_config(main_log_config);

    if (FD_ISSET(thread_msg_fd,&errorfds)) { // more tedium
      error("Error on thread_msg pipe. Exiting.");
      unexpected_exit(90,"thread_msg_fd");
    }

    for (ssh_tunnel *ssh=ssh_tunnel_list; ssh; ssh=ssh->next) {
      if (ssh->parent_stdout_fd >= 0 && FD_ISSET(ssh->parent_stdout_fd,&errorfds)) {
        error("Error on child stdout pipe. Exiting.");
        unexpected_exit(93,"child stdout"); // IMPROVEMENT: just kill+reset the child
      }
      if (ssh->parent_stderr_fd >= 0 && FD_ISSET(ssh->parent_stderr_fd,&errorfds)) {
        error("Error on child stderr pipe. Exiting.");
        unexpected_exit(94,"child stderr"); // IMPROVEMENT: just kill+reset the child
      }
      if (ssh->parent_stdout_fd >= 0 && FD_ISSET(ssh->parent_stdout_fd,&readfds)) {
        read_from_child("STDOUT", ssh, ssh->parent_stdout_fd);
      }
      if (ssh->parent_stderr_fd >= 0 && FD_ISSET(ssh->parent_stderr_fd,&readfds)) {
        read_from_child("STDERR", ssh, ssh->parent_stdout_fd);
      }
    }

    if (FD_ISSET(thread_msg_fd,&readfds)) {
      char tmpbuf[1000]; 
      read(thread_msg_fd,tmpbuf,sizeof(tmpbuf)-1);
      // if we fail to read all available data, select() will wake us up again - which is fine. 
    }

    // Create new threads to handle socket connection
    thread_local_set_log_config(NULL);
    for (proxy_instance *proxy=proxy_instance_list; proxy; proxy = proxy -> next) {
      thread_local_set_proxy_instance(proxy);
      for (service *srv = proxy->service_list; srv; srv=srv->next) {
        thread_local_set_service(srv);
        if (FD_ISSET(srv->fd,&readfds)) {
          client_connection *con = accept_connection(srv);
          if (con != NULL) {
            proxy->client_connection_list = insert_client_connection(proxy->client_connection_list, con);
            thread_local_set_client_connection(con);
            char tmpbuf[2000]; 
            debug("New connection from %s",client_connection_str(con,tmpbuf,sizeof(tmpbuf)));
            launch_thread(proxy, srv, con);
            thread_local_set_client_connection(NULL);
          }
        }
      }
    }
    thread_local_set_service(NULL);
    thread_local_set_proxy_instance(NULL);
    thread_local_set_log_config(main_log_config);

    // clean up any exited / deleted connections
    thread_local_set_log_config(NULL);
    for (proxy_instance *proxy=proxy_instance_list; proxy; proxy = proxy -> next) {
      thread_local_set_proxy_instance(proxy);
      proxy->client_connection_list = cleanup_connections(proxy->client_connection_list);
    }
    thread_local_set_proxy_instance(NULL);
    thread_local_set_log_config(main_log_config);

    // check on SSH tunnels
    check_ssh_tunnels(proxy_instance_list, ssh_tunnel_list);

    // check if anyone requested a JSON blob of the current connection state
    int needJSON=0;
    for (proxy_instance *proxy=proxy_instance_list; proxy && needJSON==0; proxy = proxy -> next) {
      for (client_connection *con = proxy->client_connection_list; con && needJSON==0; con=con->next) {
        if (con->JSONStatusRequested && !con->JSONStatusReady) { 
          needJSON=1;
        }
      }
    }
    if (needJSON) {
      char *json=build_json(proxy_instance_list, proxy_start_time, ssh_tunnel_list);
      long jsonLen=0;
      if (json != NULL) {
        jsonLen=strlen(json);
        trace2("Got some JSON for ya (%li bytes)", jsonLen);
      } else {
        trace2("No JSON For us :(");
      }
      thread_local_set_log_config(NULL);
      for (proxy_instance *proxy=proxy_instance_list; proxy ; proxy = proxy -> next) {
        thread_local_set_proxy_instance(proxy);
        for (client_connection *con = proxy->client_connection_list; con ; con=con->next) {
          if (con->JSONStatusRequested && !con->JSONStatusReady) { 
            if (json != NULL) {
              con->JSONStatusStr=strdup(json);
              free(strdup(json));
            } else {
              con->JSONStatusStr=NULL;
            }
            con->JSONStatusLen=jsonLen;
            con->JSONStatusReady=1;
          }
        }
      }
      thread_local_set_proxy_instance(NULL);
      thread_local_set_log_config(main_log_config);
      if (json) {
        free(json);
      } 
    } 

    for (log_file* log=log_file_list; log; log=log->next) {
      log_file_rotate(log);
    }
  } 


  trace("Main loop exited.");
  return 0;
}

