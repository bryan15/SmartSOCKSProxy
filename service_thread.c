// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdlib.h>
#include<stdio.h>
#include<strings.h>

#include"service.h"
#include"log.h"
#include"client_connection.h"
#include"listen_socket.h"
#include"thread_local.h"
#include"safe_close.h"

void service_thread_setup(void *data) {
  thread_data *tdata = data;
  thread_local_set_proxy_instance(tdata->proxy);
  thread_local_set_service(tdata->srv);
  thread_local_set_client_connection(tdata->con);
}

void service_thread_shutdown(client_connection *con, int ok) {
  char buf[100];
  debug("CLOSE connection %s %s  Tx %llu Rx %llu bytes",client_connection_str(con,buf,sizeof(buf)),ok?"":" due to error", con->bytes_tx, con->bytes_rx);

  if (!ok && con->status == CCSTATUS_OKAY) {
    set_client_connection_status(con,CCSTATUS_ERROR,"Error","Unknown error; closing connction.");
  }

  // we do this here instead of leaving it for when the data structure is free'ed
  // because we want to close the network connections quickly to remain responsive
  // to the connected applications. 
  safe_close(con, con->fd_in);
  con->fd_in=-1;
  safe_close(con, con->fd_out);
  con->fd_out=-1;

  con->end_time=time(NULL);
  con->thread_has_exited=1;
}

