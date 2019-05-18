// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<sys/errno.h>
#include<pthread.h>

#include"thread_local.h"

pthread_key_t thread_local_key_proxy_instance;
pthread_key_t thread_local_key_service;
pthread_key_t thread_local_key_client_connection;

int thread_local_init_key(pthread_key_t* key) {
  int rc = 0;
  do {
    rc = pthread_key_create(key,NULL);
  } while (rc<0 && (errno==EINTR || errno==EAGAIN));
  if (rc<0) {
    perror("Could not initialize thread-local storage: ");
  }
  return rc;
}

int thread_local_init(void) {
  int rc = 0;
  if (rc == 0) {  rc=thread_local_init_key(&thread_local_key_proxy_instance);  }
  if (rc == 0) {  rc=thread_local_init_key(&thread_local_key_service);  }
  if (rc == 0) {  rc=thread_local_init_key(&thread_local_key_client_connection);  }
  return rc;
}

int thread_local_set_proxy_instance(proxy_instance *inst) {
  return pthread_setspecific(thread_local_key_proxy_instance,inst);
}

proxy_instance* thread_local_get_proxy_instance(void) {
  return pthread_getspecific(thread_local_key_proxy_instance);
}

int thread_local_set_service(service *srv) {
  return pthread_setspecific(thread_local_key_service,srv);
}

service* thread_local_get_service(void) {
  return pthread_getspecific(thread_local_key_service);
}

int thread_local_set_client_connection(client_connection *inst) {
  return pthread_setspecific(thread_local_key_client_connection,inst);
}

client_connection* thread_local_get_client_connection(void) {
  return pthread_getspecific(thread_local_key_client_connection);
}

