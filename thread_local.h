#ifndef THREAD_LOCAL_H
#define THREAD_LOCAL_H

// Thread-Local Storage

#include<pthread.h>

#include"proxy_instance.h"
#include"service.h"
#include"client_connection.h"

// this is used to pass into the thread any data it needs to run properly
typedef struct thread_data {
  proxy_instance *proxy; 
  service *srv; 
  client_connection *con;
} thread_data;

int thread_local_init(void);

int thread_local_set_proxy_instance(proxy_instance *inst);
proxy_instance* thread_local_get_proxy_instance(void);

int thread_local_set_service(service *srv);
service* thread_local_get_service(void);

int thread_local_set_client_connection(client_connection *inst);
client_connection* thread_local_get_client_connection(void);

#endif // THREAD_LOCAL_H
