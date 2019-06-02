// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef CLIENT_CONNECTION_H
#define CLIENT_CONNECTION_H

#include<pthread.h>
#include<netinet/in.h>
#include<time.h>

#include"service.h"
#include"ssh_tunnel.h"
#include"host_id.h"
#include"route_rule.h"

#define CCSTATUS_OKAY         0
#define CCSTATUS_ERROR        1
#define CCSTATUS_ERR_INTERNAL 2
#define CCSTATUS_ERR_NETWORK  3

typedef struct client_connection {
  struct client_connection *prev,*next;
  unsigned long long  id;

  pthread_mutex_t mutex;

  service *srv; // service this connection is associated with

  int        fd_in;  // client connection calling us (fd = file descriptor)
  int        fd_out; // our out-going connection to the service or downstream socks5 server 

  // thread stuff
  int        pthread_create_called;
  int        pthread_create_value;
  pthread_t  thread_id;
  int        thread_should_exit; // message from server -> thread that thread should exit.
  int        thread_has_exited;  // message from thread -> server that thread is done.
  
  host_id src_host;

  // metrics
  unsigned long long bytes_tx; // client -> server        ** USE MUTEX 
  unsigned long long bytes_rx; // server -> client        ** USE MUTEX
  time_t start_time; // time of connection creation
  time_t end_time;   // time when connection was closed

  // the primary use of status* fields is to communicate
  // internal status & state to the web ui. Not to be used by smartsocksproxy.
  int status;                   // ** USE MUTEX non-zero if an error was detected on the connection 
  char statusName[128];         // ** USE MUTEX
  char statusDescription[1024]; // ** USE MUTEX

  ////////////////////////////////////////////
  // The stuff below is a bit weird. 
  // It's stuff that's specific to a single service. Then why is it here?
  // Two reasons: 
  //    - it needs to be associated with the connection.
  //      (then why don't we extend client_connection? because...)
  //    - um.. more polymorphism in C? How about we just put it here.

  /////// Routing
  // The routing rule we matched against
  route_rule *route; 

  /////// SOCKS-related variables
  // All socks-related stuff changes by the connection thread - ** USE MUTEX **
  ssh_tunnel *tunnel;     // if non-null, indicates the tunnel we successfully connected to. 
  int socks_version; 
  int socks_command, socks_command_original;
  int socks_address_type, socks_address_type_original;
  host_id dst_host, dst_host_original; // destination

  /////// HTTP-related variables
  char urlPath[4096];
  int JSONStatusRequested; // thread -> main loop (written by thread, read by main loop)
  int JSONStatusReady;     // main loop -> thread 
  char* JSONStatusStr;     // main loop -> thread (safe to read when JSONStatusReady = 1)   
  long JSONStatusLen;      // main loop -> thread (safe to read when JSONStatusReady = 1)

} client_connection;

client_connection *new_client_connection();
client_connection *insert_client_connection(client_connection *head, client_connection *con);
void remove_client_connection(client_connection *con);
void free_client_connection(client_connection *con);

void lock_client_connection(client_connection *con);
void unlock_client_connection(client_connection *con);
void set_client_connection_status(client_connection *con, int error, char *errorName, char *errorDescription);

char *client_connection_str(client_connection *con, char *buf, int buflen);

#endif // CLIENT_CONNECTION_H


