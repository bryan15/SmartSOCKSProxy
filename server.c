// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include<poll.h>
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
#include"server.h"
#include"main_config.h"

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
    info("%s %s: %s",ssh->name, label,tmpbuf);
  }
}

void set_pfd(int ulimit, struct pollfd pfd[], int *index, int fd) {
  if (*index < 0 || *index >= ulimit) {
    unexpected_exit(95,"set_pfd()"); // FIXME TODO
  }
  pfd[*index].fd = fd;
  pfd[*index].events = POLLRDNORM;
  pfd[*index].revents = 0;
  (*index)++;
}
int find_pfd(struct pollfd pfd[], int max, int fd) { // FIXME TODO this is very inefficient
  if (fd < 0) {
    return -1;
  }
  for (int i=0; i<max; i++) {
    if (pfd[i].fd == fd) {
      return i;
    }
  }
  return -1;
}

// Main loop
//int server(int port, port_forward *port_forward_pool) {
int server(log_file* log_file_list, proxy_instance* proxy_instance_list, ssh_tunnel* ssh_tunnel_list, main_config *main_conf) {

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
  // (ie: break out of blocking poll())
  // For this purpose, we create a pipe. This pipe is included
  // in poll(). To wake up the main thread, write a byte to the
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
  thread_local_set_log_config(&main_conf->log);

  time_t proxy_start_time = time(NULL);


  struct pollfd *pfd;
  int pfd_max;
  int pfd_idx;
  int rc;
  int timeout;
  struct sockaddr_in client_addr;
  client_connection *pool=NULL;

  pfd=malloc(sizeof(struct pollfd) * main_conf->ulimit +1); 
  if (pfd < 0) {
    errorNum("malloc() for pollfd");
    unexpected_exit(87,"malloc()");
  }

  while (!exit_server) { 
    //trace2("loop"); // some things are too much even for trace2

    pfd_max=0;

    set_pfd(main_conf->ulimit, pfd, &pfd_max, thread_msg_fd);

    for (proxy_instance *proxy=proxy_instance_list; proxy; proxy = proxy -> next) {
      for (service *srv = proxy->service_list; srv; srv=srv->next) {
        set_pfd(main_conf->ulimit, pfd, &pfd_max, srv->fd);
      }
    }

    for (ssh_tunnel *ssh=ssh_tunnel_list; ssh; ssh=ssh->next) {
      set_pfd(main_conf->ulimit, pfd, &pfd_max, ssh->parent_stdout_fd);
      set_pfd(main_conf->ulimit, pfd, &pfd_max, ssh->parent_stderr_fd);
    }

    // this is arbitrary... no magic here. 
    // just wake up periodically and reap dead threads.
    timeout = 1200;

    //trace("poll()...");
    do {
      rc = poll(pfd,pfd_max,timeout);
    } while (errno == EAGAIN);
    //trace("... poll() returned %i",rc);
    if (rc <0 && errno != EINTR) {
      errorNum("listen poll()");
      unexpected_exit(87,"poll()");
    }
    
    for (int i=0;i<pfd_max;i++) {
      short tmp = pfd[i].revents;
      tmp |= POLLRDNORM; 
      tmp ^= POLLRDNORM; 
      if (tmp != 0) {
        trace("poll() %i  fd %i revents = %s %s %s %s %s %s %s %s\n", i, pfd[i].fd,
          pfd[i].revents & POLLERR ? "POLLERR" : "",
          pfd[i].revents & POLLHUP ? "POLLHUP" : "",
          pfd[i].revents & POLLNVAL ? "POLLNVAL" : "",
          pfd[i].revents & POLLPRI ? "POLLPRI" : "",
          pfd[i].revents & POLLRDBAND ? "POLLRDBAND" : "",
          pfd[i].revents & POLLRDNORM ? "POLLRDNORM" : "",
          pfd[i].revents & POLLWRBAND ? "POLLWRBAND" : "",
          pfd[i].revents & POLLWRNORM ? "POLLWRNORM" : ""
          );
      }
    }

    for (ssh_tunnel *ssh=ssh_tunnel_list; ssh; ssh=ssh->next) {
      pfd_idx = find_pfd(pfd,pfd_max,ssh->parent_stdout_fd);
      if (pfd_idx >=0 && pfd[pfd_idx].revents & (POLLERR | POLLNVAL)) {
        error("Error on child stdout pipe. Exiting.");
        unexpected_exit(93,"child stdout"); // IMPROVEMENT: just kill+reset the child
      }
      if (pfd_idx >=0 && pfd[pfd_idx].revents & POLLRDNORM) {
        read_from_child("STDOUT", ssh, ssh->parent_stdout_fd);
      }
      pfd_idx = find_pfd(pfd,pfd_max,ssh->parent_stderr_fd);
      if (pfd_idx >=0 && pfd[pfd_idx].revents & (POLLERR | POLLNVAL)) {
        error("Error on child stderr pipe. Exiting.");
        unexpected_exit(94,"child stderr"); // IMPROVEMENT: just kill+reset the child
      }
      if (pfd_idx >=0 && pfd[pfd_idx].revents & POLLRDNORM) {
        read_from_child("STDERR", ssh, ssh->parent_stderr_fd);
      }
    }

    pfd_idx = find_pfd(pfd,pfd_max,thread_msg_fd);
    if (pfd_idx < 0) {
     error("Did not find thread_msg_fd in pfd[]. This should not happen");
    }
    if (pfd_idx >=0 && pfd[pfd_idx].revents & (POLLERR | POLLHUP | POLLNVAL)) {
      error("Error on thread_msg pipe. Exiting.");
      unexpected_exit(90,"thread_msg_fd");
    }
    if (pfd_idx >=0 && pfd[pfd_idx].revents & POLLRDNORM) {
      char tmpbuf[1000]; 
      read(thread_msg_fd,tmpbuf,sizeof(tmpbuf)-1);
      // if we fail to read all available data, poll() will wake us up again - which is fine. 
    }

    // Create new threads to handle socket connection
    thread_local_set_log_config(NULL);
    for (proxy_instance *proxy=proxy_instance_list; proxy; proxy = proxy -> next) {
      for (service *srv = proxy->service_list; srv; srv=srv->next) {
        pfd_idx = find_pfd(pfd,pfd_max,srv->fd);
        if (pfd_idx < 0) {
           error("Did not find service type %i in pfd[]. This should not happen", srv->type);
        }
        if (pfd >=0 && pfd[pfd_idx].revents & (POLLERR | POLLHUP | POLLNVAL)) {
          thread_local_set_proxy_instance(proxy); // for log messages
          thread_local_set_service(srv); // for log messages
          error("Error on listen socket. Exiting.");
          unexpected_exit(88,"service fd error"); // FIXME: should handle this more elegantly. Maybe no need to exit. But it should never happen if we properly handle all events. 
        }
        if (pfd_idx >=0 && pfd[pfd_idx].revents & (POLLPRI | POLLRDBAND | POLLRDNORM)) {
          thread_local_set_proxy_instance(proxy);
          thread_local_set_service(srv);
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
    thread_local_set_log_config(&main_conf->log);

    // clean up any exited / deleted connections
    thread_local_set_log_config(NULL);
    for (proxy_instance *proxy=proxy_instance_list; proxy; proxy = proxy -> next) {
      thread_local_set_proxy_instance(proxy);
      proxy->client_connection_list = cleanup_connections(proxy->client_connection_list);
    }
    thread_local_set_proxy_instance(NULL);
    thread_local_set_log_config(&main_conf->log);

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
      thread_local_set_log_config(&main_conf->log);
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

