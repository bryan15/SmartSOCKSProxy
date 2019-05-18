#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <arpa/inet.h>

#include"build_json.h"
#include"proxy_instance.h"
#include"client_connection.h"
#include"service.h"
#include"service_http.h"
#include"service_port_forward.h"
#include"service_socks.h"
#include"version.h"
#include"socks5.h"

long default_size=1024;

// A bit clumsy, but gets the job done.
void add_to_buf(char **buf_in, int *buf_size_in, char **ptr_in, char *add) {
  char *buf, *ptr;
  int buf_len, add_len, buf_size;
 
  buf = *buf_in;
  ptr = *ptr_in;
  buf_size = *buf_size_in;
  
  add_len = strlen(add);
  buf_len = ptr-buf;
 
  int new_size = buf_size;
  while (new_size < (buf_len + add_len + 10)) { // deliberately trigger a realloc() a little early, just to keep me sane
    new_size *= 2;
  }

  if (buf_size != new_size) {  
    char *tmp = realloc(buf, new_size);
    if (tmp != NULL) {
      buf = tmp;
      buf_size = new_size;
      ptr = buf + buf_len;
    }
  }

  if ( buf_size > buf_len + add_len ) { 
    strncpy(ptr,add,add_len);
    ptr += add_len;
    *ptr=0;
  }

  // return new values
  *buf_in = buf;
  *buf_size_in = buf_size;
  *ptr_in = ptr;
}

void add_to_buf_int(char **buf_in, int *buf_size_in, char **ptr_in, long long value) {
  char tmp[500];
  snprintf(tmp,sizeof(tmp)-1,"%lli",value);
  add_to_buf(buf_in,buf_size_in,ptr_in,tmp);
}

void add_to_buf_uint(char **buf_in, int *buf_size_in, char **ptr_in, long long unsigned value) {
  char tmp[500];
  snprintf(tmp,sizeof(tmp)-1,"%llu",value);
  add_to_buf(buf_in,buf_size_in,ptr_in,tmp);
}

/////

void add_string(char **buf, int *size, char **ptr, char *name, char *value) {
  add_to_buf(buf,size,ptr,"\"");
  add_to_buf(buf,size,ptr,name);
  add_to_buf(buf,size,ptr,"\":\"");
  add_to_buf(buf,size,ptr,value);
  add_to_buf(buf,size,ptr,"\"");
}
void add_int(char **buf, int *size, char **ptr, char *name, long long value) {
  add_to_buf(buf,size,ptr,"\"");
  add_to_buf(buf,size,ptr,name);
  add_to_buf(buf,size,ptr,"\":");
  add_to_buf_int(buf,size,ptr,value);
}
void add_uint(char **buf, int *size, char **ptr, char *name, long long unsigned value) {
  add_to_buf(buf,size,ptr,"\"");
  add_to_buf(buf,size,ptr,name);
  add_to_buf(buf,size,ptr,"\":");
  add_to_buf_uint(buf,size,ptr,value);
}
void add_comma(char **buf, int *size, char **ptr) {
  add_to_buf(buf,size,ptr,",");
}

////////////////////////// SERVICE

void add_service(char **buf, int *size, char **ptr, service *srv) {
  add_to_buf(buf,size,ptr,"\"");
  add_to_buf_uint(buf,size,ptr,srv->id);
  add_to_buf(buf,size,ptr,"\":{");

  add_uint(buf,size,ptr,"serviceId",srv->id);
  add_comma(buf,size,ptr);
   
  add_to_buf(buf,size,ptr,"\"type\":\"");
  if (srv->type == SERVICE_TYPE_NONE)         { add_to_buf(buf,size,ptr,"none"); }
  if (srv->type == SERVICE_TYPE_SOCKS)        { add_to_buf(buf,size,ptr,"SOCKS"); }
  if (srv->type == SERVICE_TYPE_PORT_FORWARD) { add_to_buf(buf,size,ptr,"portForward"); }
  if (srv->type == SERVICE_TYPE_HTTP)         { add_to_buf(buf,size,ptr,"HTTP"); }
  add_to_buf(buf,size,ptr,"\",");

  if (srv->type == SERVICE_TYPE_PORT_FORWARD) {
    service_port_forward *fwd = (void*)srv;
    add_string(buf,size,ptr,"remoteAddress",fwd->remote_host);
    add_comma(buf,size,ptr);
    add_int(buf,size,ptr,"remotePort",fwd->remote_port);
    add_comma(buf,size,ptr);
  }
  if (srv->type == SERVICE_TYPE_HTTP) {
    service_http *http = (void*)srv;
    add_to_buf(buf,size,ptr,"\"baseDir\":\"");
    add_to_buf(buf,size,ptr,http->base_dir);
    add_to_buf(buf,size,ptr,"\",");
  }
 
  add_to_buf(buf,size,ptr,"\"localAddress\":\"");
  add_to_buf(buf,size,ptr,srv->bind_address);
  add_to_buf(buf,size,ptr,"\",");
  add_to_buf(buf,size,ptr,"\"localPort\":");
  add_to_buf_int(buf,size,ptr,srv->port);

  add_to_buf(buf,size,ptr,"}");
}

////////////////////////// CLIENT_CONNECTION

void add_client_connection(char **buf, int *size, char **ptr, client_connection *con_in) {
  char tmp[1024];
  client_connection *con, con_local;

  con=&con_local;
 
  // capture a snapshot of this connection
  lock_client_connection(con_in);
  *con = *con_in;
  unlock_client_connection(con_in);
  
  service *srv = con->srv;

  add_to_buf(buf,size,ptr,"\"");
  add_to_buf_uint(buf,size,ptr,con->id);
  add_to_buf(buf,size,ptr,"\":{"); // this connection

  add_uint(buf,size,ptr,"connectionId",con->id);
  add_comma(buf,size,ptr);

  add_to_buf(buf,size,ptr,"\"service\":");
  add_to_buf_uint(buf,size,ptr,srv->id);
  add_to_buf(buf,size,ptr,",");

  if ((srv->type == SERVICE_TYPE_SOCKS || srv->type == SERVICE_TYPE_PORT_FORWARD) && con->tunnel != NULL) {
    add_to_buf(buf,size,ptr,"\"route\":\"");
    add_to_buf(buf,size,ptr,con->tunnel->name);
    add_to_buf(buf,size,ptr,"\",");
  
    add_int(buf,size,ptr,"socksVersion",con->socks_version);
    add_comma(buf,size,ptr);
    add_int(buf,size,ptr,"socksCommand",con->socks_command);
    add_comma(buf,size,ptr);

    add_string(buf,size,ptr,"remoteAddress",host_id_str(&(con->dst_host_original),tmp,sizeof(tmp)-1));
    add_comma(buf,size,ptr);
    add_int(buf,size,ptr,"remotePort",host_id_get_port(&(con->dst_host_original)));
    add_comma(buf,size,ptr);
    add_string(buf,size,ptr,"remoteAddressEffective",host_id_str(&(con->dst_host),tmp,sizeof(tmp)-1));
    add_comma(buf,size,ptr);
    add_int(buf,size,ptr,"remotePortEffective",host_id_get_port(&(con->dst_host)));
    add_comma(buf,size,ptr);
  }

  add_int(buf,size,ptr,"status",con->status);
  add_comma(buf,size,ptr);
  if (con->statusName[0]) {
    add_string(buf,size,ptr,"statusName",con->statusName);
    add_comma(buf,size,ptr);
  }
  if (con->statusDescription[0]) {
    add_string(buf,size,ptr,"statusDescription",con->statusDescription);
    add_comma(buf,size,ptr);
  }

  add_to_buf(buf,size,ptr,"\"bytesTx\":");
  add_to_buf_uint(buf,size,ptr,con->bytes_tx);
  add_to_buf(buf,size,ptr,",");

  add_to_buf(buf,size,ptr,"\"bytesRx\":");
  add_to_buf_uint(buf,size,ptr,con->bytes_rx);
  add_to_buf(buf,size,ptr,",");

  if (srv->type == SERVICE_TYPE_HTTP) {
    if (strlen(con->urlPath)>0) {
      add_string(buf,size,ptr,"urlPath",con->urlPath);
      add_comma(buf,size,ptr);
    }
  }

  add_string(buf,size,ptr,"sourceAddress",host_id_str(&(con->src_host),tmp,sizeof(tmp)-1));
  add_comma(buf,size,ptr);
  add_int(buf,size,ptr,"sourcePort",host_id_get_port(&(con->src_host)));
  add_comma(buf,size,ptr);

  if (con->end_time>0) {
    add_to_buf(buf,size,ptr,"\"timeEnd\":");
    add_to_buf_uint(buf,size,ptr,con->end_time);
    add_to_buf(buf,size,ptr,",");
  }

  add_to_buf(buf,size,ptr,"\"timeStart\":");
  add_to_buf_uint(buf,size,ptr,con->start_time);


  add_to_buf(buf,size,ptr,"}"); // this connection
}

////////////////////////// PROXY_INSTANCE

void add_proxy_instance(char **buf, int *size, char **ptr, proxy_instance *proxy) {
  add_to_buf(buf,size,ptr,"\"");
  add_to_buf(buf,size,ptr,proxy->name); // this proxy_instance
  add_to_buf(buf,size,ptr,"\":{");
 
  add_string(buf,size,ptr,"name",proxy->name);
  add_comma(buf,size,ptr);
 
  add_to_buf(buf,size,ptr,"\"logLevel\":"); 
  add_to_buf_int(buf,size,ptr,proxy->log_level);
  add_to_buf(buf,size,ptr,","); 

  add_to_buf(buf,size,ptr,"\"logMaxSize\":"); 
  add_to_buf_int(buf,size,ptr,proxy->log_max_size);
  add_to_buf(buf,size,ptr,","); 

  add_to_buf(buf,size,ptr,"\"logMaxRotate\":"); 
  add_to_buf_int(buf,size,ptr,proxy->log_max_rotate);
  add_to_buf(buf,size,ptr,","); 
    
  add_to_buf(buf,size,ptr,"\"logFilename\":\""); 
  add_to_buf(buf,size,ptr,proxy->log_file_name);
  add_to_buf(buf,size,ptr,"\","); 

  add_to_buf(buf,size,ptr,"\"service\":{"); // service
  int needComma = 0;
  for (service *srv = proxy->service_list; srv ; srv=srv->next) {
    if (needComma) add_to_buf(buf,size,ptr,","); // bloody json doesn't allow trailing comma's
    needComma=1;
    add_service(buf,size,ptr,srv);
  }
  add_to_buf(buf,size,ptr,"},"); // service

  add_to_buf(buf,size,ptr,"\"connection\":{"); // connection
  needComma = 0;
  for (client_connection *con = proxy->client_connection_list; con ; con=con->next) {
    if (needComma) add_to_buf(buf,size,ptr,","); // bloody json doesn't allow trailing comma's
    needComma=1;
    add_client_connection(buf,size,ptr,con);
  }
  add_to_buf(buf,size,ptr,"}"); // connection
  add_to_buf(buf,size,ptr,"}"); // this proxy_instance
}

////////////////////////// 

char *build_json(proxy_instance *proxy_instance_list, time_t proxy_start_time) {
  int size = default_size;
  char *ptr; // tracks end of string to make appending faster
  char *buf=malloc(size);

  if (buf == NULL) return NULL;

  ptr=buf;
  *ptr=0;

  add_to_buf(&buf,&size,&ptr,"{"); // main

  add_int(&buf,&size,&ptr,"currentTime",time(NULL));
  add_comma(&buf,&size,&ptr);
  add_int(&buf,&size,&ptr,"proxyStartTime",proxy_start_time);
  add_comma(&buf,&size,&ptr);
  add_string(&buf,&size,&ptr,"version",SMARTSOCKSPROXY_VERSION);
  add_comma(&buf,&size,&ptr);
  add_string(&buf,&size,&ptr,"buildDate",SMARTSOCKSPROXY_BUILD_DATE);
  add_comma(&buf,&size,&ptr);
  add_string(&buf,&size,&ptr,"gitHash",SMARTSOCKSPROXY_GIT_HASH);
  add_comma(&buf,&size,&ptr);

  // proxy_instance section
  add_to_buf(&buf,&size,&ptr,"\"proxyInstance\":{");
  int needComma = 0;
  for (proxy_instance *proxy=proxy_instance_list; proxy ; proxy = proxy -> next) {
    if (needComma) add_to_buf(&buf,&size,&ptr,","); // bloody json doesn't allow trailing comma's
    needComma=1;
    add_proxy_instance(&buf,&size,&ptr, proxy);
  }
  add_to_buf(&buf,&size,&ptr,"}"); // proxy_instance

  add_to_buf(&buf,&size,&ptr,"}"); // main 

  default_size=size;

  return buf;
}

