// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include<netdb.h>
#include<netinet/in.h>
#include<sys/select.h>
#include<sys/wait.h>
#include<errno.h>
#include<stdio.h>

#include"log.h"

// returns a socket listening on the given interface and port
int listen_socket(char *listenInterface, int port) {
 
  // open listening socket
  int listen_fd;

  listen_fd= socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd <0) {
    errorNum("Cannot open listening socket");
    unexpected_exit(31,"socket()");
  }

  int flag = 1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
    errorNum("setsockopt(SO_REUSEADDR) failed");
    unexpected_exit(32,"setsockopt()");
  }
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag)) == -1) {
    errorNum("setsockopt(SO_REUSEPORT) failed");
    unexpected_exit(33,"setsockopt()");
  }

  // seems we periodically get a SIGPIPE. Example error message:
  //   INFO   9471: Routed connection 9470 127.0.0.1:59196 -> CONNECT via direct 35.161.16.31:443
  //   INFO   9470: Routed connection 9471 127.0.0.1:59197 -> CONNECT via direct 35.161.16.31:443
  //   ERROR:  Caught signal 13
  //   ERROR:  Error on listen socket. Exiting. in server.c line 262
  //   write(): Broken pipe in safe_blocking_readwrite.c line 81
  //   ERROR  9468: write(): Broken pipe in shuttle.c line 28
  // Problem is: the signal kills the application. 
  // So we just ignore it; we disable it on the socket. See https://stackoverflow.com/questions/108183/how-to-prevent-sigpipes-or-handle-them-properly
  flag=1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&flag, sizeof(int)) == -1) {
    errorNum("setsockopt(SO_NOSIGPIPE) failed");
    unexpected_exit(34,"setsockopt()");
  }

  // Initialize socket structure 
  struct sockaddr_in   server_addr;

  // security check
  if (strncmp(listenInterface,"127.",4) != 0) {
    error("listenInterface is set to %s",listenInterface);
    error("I'm not sure what you're trying to do, but listening on any interface");
    error("other than the loopback interface poses a serious security risk.");
    unexpected_exit(35,"invalid listen interface"); 
  }

  struct hostent *host = gethostbyname(listenInterface);
  bzero((char *) &server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr = *((struct in_addr *)host->h_addr);
  server_addr.sin_port = htons(port);

  /* Now bind the host address using bind() call.*/
  if (bind(listen_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
     errorNum("cannot bind() server to port %i",port);
     unexpected_exit(36,"bind()");
  }

  if (listen(listen_fd,10) < 0) {
     errorNum("cannot listen() on server socket");
     unexpected_exit(37,"listen()");
  }
  info("Listening on %s:%i",listenInterface,port);

  return listen_fd;
} 
