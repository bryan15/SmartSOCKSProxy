#include<stdlib.h>
#include<stdio.h>
#include<strings.h>

#include"service.h"
#include"log.h"
#include"client_connection.h"
#include"listen_socket.h"
#include"thread_local.h"
#include"safe_close.h"

unsigned long long service_id_pool=0;

char *service_default_str(service *srv, char *buf, int buflen) {
  char *typeStr = "NONE"; 

  if (srv->type == SERVICE_TYPE_SOCKS) { typeStr = "SOCKS"; }
  else if (srv->type == SERVICE_TYPE_PORT_FORWARD) { typeStr = "FWD"; }
  else if (srv->type == SERVICE_TYPE_HTTP) { typeStr = "HTTP"; }
  else { typeStr = "Unknown/Error"; }

  char tmp[1000]; 
  // TODO FIXME: include bind address
  snprintf(buf, buflen-1, "%llu:%s:%i", srv->id, typeStr, srv->port);
  return buf;
}

void *service_default_connection_handler(void *data) {
  thread_data *tdata = data;
  proxy_instance *proxy = tdata->proxy;
  service *srv = tdata->srv;
  client_connection *con = tdata->con;
  free(data);  // this is our responsibility.
  thread_local_set_proxy_instance(proxy);
  thread_local_set_service(srv);
  thread_local_set_client_connection(con);

  error("This is the default service connection handler and should never be used. Something is broken.");

  safe_close(con, con->fd_in);
  con->fd_in=-1;
  safe_close(con, con->fd_out);
  con->fd_out=-1;

  con->thread_has_exited=1;
  return NULL;
} 

service *new_service(int size, int type) {
  service_id_pool++;
  trace("new_service(%llu)",service_id_pool);
  service *srv =  malloc(size);
  if (srv == NULL) {
    errorNum("Error allocating new service");
    unexpected_exit(20,"Error allocating new service");
  }
  srv->next=NULL;
  srv->id=service_id_pool;
  srv->type=type;
  srv->bind_address[0]=0;
  srv->port=0;
  srv->fd=-1;
  srv->str = &service_default_str;
  srv->connection_handler = &service_default_connection_handler;
  return srv;
}

service *insert_service(service *head, service *srv) {
  trace("insert_service()");
  // srv is the new head
  srv->next=head;
  return srv;
}

void service_listen(service *srv) {
  char buf[1000];
  info("Creating listening socket for %s", (*srv->str)(srv,buf,sizeof(buf)));
  srv->fd = listen_socket(srv->bind_address, srv->port);
}

