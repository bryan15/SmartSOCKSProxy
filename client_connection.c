// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdlib.h>
#include<unistd.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<strings.h>
#include<time.h>
#include<pthread.h>
#include<sys/errno.h>

#include"log.h"
#include"client_connection.h"
#include"ssh_tunnel.h"
#include"ssh_tunnel.h"
#include"route_rule.h"

unsigned long long id_pool=0;

client_connection *new_client_connection() {
  id_pool++;

  trace2("new_client_connection(%llu)",id_pool);
  client_connection *con =  malloc(sizeof(struct client_connection));
  if (con == NULL) {
    errorNum("Error allocating new client_connection");
    unexpected_exit(40,"malloc()"); 
  }
  con->id=id_pool;
  pthread_mutex_init(&(con->mutex),NULL);
  con->prev=con->next=NULL;
  con->srv=NULL;
  con->fd_in=-1;
  con->fd_out=-1;
  con->thread_should_exit=0;
  con->thread_has_exited=0;
  con->pthread_create_called=0;
  con->bytes_rx=0;
  con->bytes_tx=0;
  con->start_time=time(NULL);
  con->end_time=0;
  con->status=CCSTATUS_OKAY;
  con->statusName[0]=0;
  con->statusDescription[0]=0;
  host_id_init(&(con->src_host));

  // service-specific variables
  con->route=NULL;
  con->tunnel=NULL;
  con->urlPath[0]=0;
  host_id_init(&(con->dst_host));
  host_id_init(&(con->dst_host_original));
  con->JSONStatusRequested=0;
  con->JSONStatusReady=0;
  con->JSONStatusStr=NULL;
  con->JSONStatusLen=0;

  return con;
}

client_connection *insert_client_connection(client_connection *head, client_connection *con) {
  trace2("insert_client_connection()");
  // con is the new head
  con->next=head;
  con->prev=NULL;
  if (head != NULL) {
    head->prev=con;
  }
  return con;
}
 
void remove_client_connection(client_connection *con) {
  trace2("remove_client_connection()");
  if (con->next != NULL) {
    con->next->prev=con->prev;
  }
  if (con->prev != NULL) {
    con->prev->next=con->next;
  }
  con->next = NULL;
  con->prev = NULL;
}

void free_client_connection(client_connection *con) {
  trace2("free_client_connection()");
  remove_client_connection(con);
  int rc;
  if (con->fd_in > -1) {
    do {
      rc = close(con->fd_in);
    } while (rc<0 && errno == EINTR);
    con->fd_in=-1;
  }
  if (con->fd_out > -1) {
    do {
      close(con->fd_out);
    } while (rc<0 && errno == EINTR);
    con->fd_out=-1;
  }
  free(con);
}

char *client_connection_str(client_connection *con, char *buf, int buflen) {
  return host_id_str(&con->src_host,buf,buflen);
}

void lock_client_connection(client_connection *con) {
  int rc;
  do {
    rc = pthread_mutex_lock(&(con->mutex));
  } while (rc == EINTR); // not sure this works as intended...
  if (rc != 0) {
    warn("lock_client_connection() returned %i\n",rc);
  }
}

void unlock_client_connection(client_connection *con) {
  int rc;
  do {
    rc=pthread_mutex_unlock(&(con->mutex));
  } while (rc == EINTR); 
}

void set_client_connection_status(client_connection *con, int status, char *statusName, char *statusDescription) {
  lock_client_connection(con);
  con->status = status; 
  con->statusName[0]=0;
  con->statusDescription[0]=0;
  if (statusName != NULL) {
    strncpy(con->statusName, statusName, sizeof(con->statusName)-1);
  }
  if (statusDescription != NULL) {
    strncpy(con->statusDescription,statusDescription, sizeof(con->statusDescription)-1);
  }
  unlock_client_connection(con);
}

