// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>
#include<errno.h>

#include"log.h"
#include"service.h"
#include"service_http.h"
#include"string2.h"
#include"thread_local.h"
#include"safe_blocking_readwrite.h"
#include"thread_msg.h"
#include"string2.h"
#include"service_thread.h"

char *service_http_str(service_http *http, char *buf, int buflen) {
  char local_buf[4096];
  snprintf(buf,buflen-1,"%llu:HTTP:%i",http->srv.id,http->srv.port);
  return buf;
}

int send_string(client_connection *con, char* str) {
  if (str == NULL) {
    return 1;
  }
  int size=strlen(str);
  if (sb_write_len(con->fd_in,(unsigned char*)str,strlen(str)) == size) {
    return 1;
  }
  return 0;
}


// read buflen or first \n, whichever comes first
// Not the most efficient; we read 1 byte at a time.
int read_one_line_safely(int fd, char *buf, size_t buflen) {
  char *ptr = buf;
  size_t len = 0;      
  
  if (buflen < 1) {
    return 0;  
  }

  int keep_reading=1;
  int rc=1;
  while(rc>0 && keep_reading && len < buflen) {
    rc=sb_read_len(fd,(unsigned char*)ptr,1);
    if (rc>0) {
      if (*ptr == '\n' || *ptr == '\r') {
        // eat the \n (by not incremeenting len)
        keep_reading=0;
      } else {
        len++;
      }
      ptr++;  
    } 
  }
  if (rc>0) {
    return len;
  }   
  return rc;
} 


int basic_response(client_connection *con, char *responseStr, char *content_type) {
  int ok=1;
  if (ok) ok=send_string(con, "HTTP/1.1 ");
  if (ok) ok=send_string(con, responseStr);
  if (ok) ok=send_string(con, "\n");
  if (ok) ok=send_string(con, "Server: SmartSOCKSProxy/1.0\n");
  if (ok) ok=send_string(con, "Connection: Closed\n");
  if (ok) ok=send_string(con, "Transfer-Encoding: identity\n");
  if (content_type != NULL) {
    if (ok) ok=send_string(con, "Content-Type: ");
    if (ok) ok=send_string(con, content_type);
    if (ok) ok=send_string(con, "\n");
  }
  //if (ok) ok=send_string(con, "Vary: Accept-Encoding\n");
  if (ok) ok=send_string(con, "\n");
  return ok;
}

char *get_safe_filename_from_request(char *request,char *filename,int len) {
  char *src = request; 
  int index=0;
  // start is 'GET /' so skip past that part...
  src += 5; 
  // okay look, we're not building Apache's replacement here. 
  // I just want this to be safe. So the characters allowed in 
  // a filename are white-listed. '/' is prohibited, along with 
  // other nefarious shell-friendly control characters. In other
  // words, static web pages have to be in a single directory. 
  // If you don't like this, **YOU** can figure out how to reliably 
  // defend against "GET /../../../../../etc/passwd".
  while (*src && *src != ' ' && index < (len -1)) { // parsing sucks.
    char chr = *src;
    if ( (chr >= 'a' && chr <= 'z') ||
         (chr >= 'A' && chr <= 'Z') ||
         (chr >= '0' && chr <= '9') ||
         (chr == '_') || 
         (chr == '-') || 
         (chr == '.') || 
         (chr == '+') || 
         (chr == ':') ) {
      filename[index]=chr; 
      index++;
    }
    src++;
  }
  filename[index]=0; 
  return filename; 
}

char *return_file_contents(proxy_instance *proxy, service_http *http, client_connection *con, char *request) {
  int ok=1;
  char *responseStr="500 Internal Server Error";
  char filename[4096];
  char *content_type=NULL;

  if (string_starts_with(request,"GET / ")) {
    strncpy(filename,"index.html",sizeof(filename));
  } else {
    get_safe_filename_from_request(request,filename,sizeof(filename)-1);
    lock_client_connection(con);
    strncpy(con->urlPath,filename,sizeof(con->urlPath)-1);
    unlock_client_connection(con);
  }
  trace("HTTP requested file: %s",filename);
  if (string_ends_with(filename,".html")) {
    content_type = "text/html; charset=utf-8";
  } else if (string_ends_with(filename,".css")) {
    content_type = "text/css; charset=utf-8";
  } else if (string_ends_with(filename,".txt")) {
    content_type = "text/plain";
  } else if (string_ends_with(filename,".js")) {
    content_type = "application/javascript; charset=utf-8";
  } else if (string_ends_with(filename,".json")) {
    content_type = "application/javascript; charset=utf-8";
  } else if (string_ends_with(filename,".xml")) {
    content_type = "text/xml";
  } else if (string_ends_with(filename,".xhtml")) {
    content_type = "application/xhtml+xml";
  } else if (string_ends_with(filename,".gif")) {
    content_type = "image/gif";
  } else if (string_ends_with(filename,".png")) {
    content_type = "image/png";
  } else if (string_ends_with(filename,".jpg")) {
    content_type = "image/jpeg";
  } else if (string_ends_with(filename,".jpeg")) {
    content_type = "image/jpeg";
  } else if (string_ends_with(filename,".svg")) {
    content_type = "image/svg+xml";
  } 
  char full_filename[9000];
  snprintf(full_filename,sizeof(full_filename)-1,"%s/%s",http->base_dir,filename);
  debug("HTTP full path + filename to open: %s",full_filename);

  if (access(full_filename,F_OK) != -1) {
    int file_fd = open(full_filename,O_RDONLY);
    if (ok && file_fd >= 0) {
      responseStr="200 OK";
      if (ok) ok=basic_response(con, responseStr, content_type);
      int rc; 
      unsigned char buf[1000];
      do {
        rc=read(file_fd,buf,sizeof(buf)-1);
        if (rc>0) {
          if (sb_write_len(con->fd_in,buf,rc) != rc) {
            ok=0;
          }
        } else if (rc == 0) {
          trace("EOF for %s",filename);
        } else {
          errorNum("read()");
        }
      } while (ok && (rc > 0 || errno == EINTR));
    } else {
      responseStr="404 Not Found";
      if (ok) ok=basic_response(con, responseStr, NULL);
    }
    if (file_fd >= 0) { 
      close(file_fd);
      file_fd=-1;
    }
  } else {
    responseStr="404 Not Found";
    if (ok) ok=basic_response(con, responseStr, NULL);
  }
  return responseStr;
}

char *return_connection_state_json(proxy_instance *proxy, service_http *http, client_connection *con, char *request) {
  int ok=1;
  char *responseStr="500 Internal Server Error";
  char *contentType=NULL;

  // setting this signals our main loop to allocate and populate con->JSONStatusStr;
  con->JSONStatusRequested=1;
  thread_msg_send(" ",5); // wake up main thread from its blocking select() 
  trace("HTTP thread Waiting for our JSON...");
  for (int i=0; i<100 && !con->JSONStatusReady; i++) {
    usleep(10000);
  }
  if (ok) ok=send_string(con, "HTTP/1.1 ");
  if (con->JSONStatusReady) {
    responseStr="200 OK";
    contentType=" application/json";
  } else {
    responseStr="503 Service Unavailable";
  }
  if (ok) ok=basic_response(con, responseStr, contentType);
  if (con->JSONStatusReady && con->JSONStatusStr != NULL) {
    if (ok) ok=send_string(con, con->JSONStatusStr);
  }
  if (con->JSONStatusStr != NULL) {
    free(con->JSONStatusStr);
    con->JSONStatusStr=NULL;
  }
  return responseStr;
}


void* service_http_connection_handler(void* data) {
  service_thread_setup(data);
  thread_data *tdata = data;
  proxy_instance *proxy = tdata->proxy;
  service_http *http=(service_http*)tdata->srv;
  client_connection *con = tdata->con;
  free(data);  // this is our responsibility.

  int err=0;
  char *responseStr=NULL;
  long responseLen=0;
  int ok=1;  // in this context, ok = socket still basically works, ok=0 means we just close the connection.
  char request[1000];

  // we read the first line of the request, and ignore the rest
  if (ok) {
    int rc=read_one_line_safely(con->fd_in,request,sizeof(request)-1);
    if (rc<=0) { 
      responseStr="500 Internal Server Error";
    } else {
      request[rc]=0; // be safe! null-terminate!
      if (string_starts_with(request,"GET /status.json ")) {
        lock_client_connection(con);
        strcpy(con->urlPath,"status.json");
        unlock_client_connection(con);
        responseStr=return_connection_state_json(proxy,http,con,request);
      } else if (string_starts_with(request,"GET /")) {
        responseStr=return_file_contents(proxy,http,con,request);
      } else {
        responseStr="400 Bad Request";
        if (ok) ok=basic_response(con, responseStr, NULL);
      } 
    } 
  }  

  char tmp_buf[1024];
  if (ok) {
    if (responseStr) {
      info("%s  \"%s\"  %li %s",client_connection_str(con,tmp_buf,sizeof(tmp_buf)), request, responseLen, responseStr);
    } else {
      info("%s  \"%s\" %li",client_connection_str(con,tmp_buf,sizeof(tmp_buf)), request, responseLen);
    }
  } else {
    info("%s  Connection aborted due to error",client_connection_str(con,tmp_buf,sizeof(tmp_buf)));
  }

  // paranoia....
  // as the sole consumer of this data, it's our responsibility to free it. 
  // I guess this could be done in free_client_connection(), but really, JSONStatusStr
  // is related to service_http, not client_connection. It's only in client_connection as an 
  // unwanted houseguest who won't leave. So let's keep the responsibility here for now. 
  if (con->JSONStatusStr != NULL) {
    free(con->JSONStatusStr);  
    con->JSONStatusStr=NULL;
  }

  service_thread_shutdown(con, 1);
  return NULL;
}

service_http *new_service_http() {
  service_http *http = (service_http*) new_service(sizeof(struct service_http), SERVICE_TYPE_HTTP);
  http->base_dir[0]=0;
  http->srv.str = (void*)&service_http_str; // void* is to bypass incompatible pointer warning. See service.h for polymorphism in C.
  http->srv.connection_handler = &service_http_connection_handler;
  return http;
}

service_http *parse_service_http_spec(char *strIn) {
  char *token;
  char *strPtr;
  char *local_copy;
  int okay = 1;
  service_http *http = NULL;

  // a paremeter with 3 parts will have ':' 2 times in it. Hence the +1
  int parts = string_count_char_instances(strIn,':') + 1;
  if (parts < 2 || parts > 3) {
    error("HttpServer must have 2 or 3 parts separated by a colon: [bind_address:]local_port:base_directory");
    error("The provided paramter \"%s\" has %i parts.",strIn,parts);
    error("Example:  127.0.0.1:123:/Users/userid/smartsocksproxy_web/  or  123:/Users/userid/smartsocksproxy_web");
    return NULL;
  } 
  trace2("HttpServer Parse: From \"%s\" got %i parts:", strIn, parts);

  // get a local copy which we can modify
  local_copy = strPtr = strdup(strIn);
  if (strPtr == NULL) {
    error("error parsing http_server");
    okay = 0;
  } 
  if (okay) {
    http = new_service_http();
  } 

  // apply default to optional first part: the local address to bind to
  int index = 0;
  if (okay && parts == 2) {
    index++;
    strncpy(http->srv.bind_address,"127.0.0.1",sizeof(http->srv.bind_address)-1);
    trace2("part %i:  %s  (default)", index,http->srv.bind_address);
  }

  while (okay && (token = strsep(&strPtr,":"))) {
    index++;
    trace2("part %i:  %s", index, token);
    switch(index) {
      case 1:
        strncpy(http->srv.bind_address,token,sizeof(http->srv.bind_address)-1);
        break;
      case 2:
        if (sscanf(token,"%i",&http->srv.port) != 1) {
          error("Error converting \"%s\" to a port number. Input parameter: \"%s\".",token,strIn);
          okay=0;
        }
        break;
      case 3:
        strncpy(http->base_dir,token,sizeof(http->base_dir)-1);
        break;
      default:
        error("error parsing http server specification");
        okay = 0;
        break;
    }
  }

  // clean up after ourselves
  if (local_copy != NULL) {
    free(local_copy);
    local_copy=NULL;
  }


  if (!okay) {
    if (http != NULL) {
      free(http);
      http=NULL;
    }
  }

  return http;
}

