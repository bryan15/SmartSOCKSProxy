#include<stdio.h>
#include<sys/socket.h>
#include <arpa/inet.h>
#include<string.h>

#include"host_id.h"

void host_id_init(host_id *id) {
  if (id != NULL) {
    id->name[0]=0;
    id->addr.sa.sa_family=AF_UNSPEC;
    id->port=0;
  }
}

int host_id_has_name(host_id *id) {
  if (id->name[0] == 0) {
    return 0; 
  }
  return 1;
}

int host_id_has_addr(host_id *id) {
  if (id->addr.sa.sa_family == AF_UNSPEC) {
    return 0;
  }
  return 1;
}

char *host_id_addr_str(host_id *id, char *buf, int len) {
  const char* result=NULL;
  if (id->addr.sa.sa_family == AF_INET) {
    result = inet_ntop(AF_INET,&(id->addr.sa_in.sin_addr),buf,len-1);
  } else if (id->addr.sa.sa_family == AF_INET6) {
    result = inet_ntop(AF_INET6,&(id->addr.sa_in6.sin6_addr),buf,len-1);
  }
  if (result == NULL) {
    return NULL;
  }
  return buf;
}

char *host_id_name_or_addr_str(host_id *id, char *buf, int len) {
  if (host_id_has_name(id)) {
    return id->name;
  }
  if (host_id_has_addr(id)) {
    return host_id_addr_str(id,buf,len);
  }
  return NULL;
}

char *host_id_str(host_id *id, char *buf, int len) {
  char tmp[1024];
  snprintf(buf,len-1,"%s:%i:%s",id->name,host_id_get_port(id),host_id_addr_str(id,tmp,sizeof(tmp)-1));
  return buf;
}

void host_id_set_name(host_id *id, char *name) {
  strncpy(id->name,name,strlen(id->name));
}

char *host_id_get_name(host_id *id) {
  return id->name;
}

void host_id_set_port(host_id *id, int port) {
  id->port = port;
  if (id->addr.sa.sa_family == AF_INET) {
    id->addr.sa_in.sin_port = port;
  } else if (id->addr.sa.sa_family == AF_INET6) {
    id->addr.sa_in6.sin6_port = port;
  }
}

int host_id_get_port(host_id *id) {
  return id->port;
}

void host_id_set_addr_in(host_id *id, struct sockaddr_in *sa_in) {
  memcpy(&(id->addr),sa_in,sizeof(struct sockaddr_in));
}

void host_id_set_addr_in_from_byte_array(host_id *id, unsigned char* ipv4, int port) {
  id->port=port;
  struct sockaddr_in* sin = &(id->addr.sa_in);
  sin->sin_len = sizeof(struct sockaddr_in);
  sin->sin_family = AF_INET;
  sin->sin_port=port;
  sin->sin_addr.s_addr = 0;
  // FIXME: network byte ordering
  sin->sin_addr.s_addr |= ipv4[0]; sin->sin_addr.s_addr <<= 8;
  sin->sin_addr.s_addr |= ipv4[1]; sin->sin_addr.s_addr <<= 8;
  sin->sin_addr.s_addr |= ipv4[2]; sin->sin_addr.s_addr <<= 8;
  sin->sin_addr.s_addr |= ipv4[3];
}


void host_id_set_addr_in6_from_byte_array(host_id *id, unsigned char* ipv6, int port) {
  id->port=port;
  struct sockaddr_in6* sin = &(id->addr.sa_in6);
  sin->sin6_len = sizeof(struct sockaddr_in6);
  sin->sin6_family = AF_INET6;
  sin->sin6_port=port;
  // FIXME: network byte ordering
  for (int i=0;i<16;i++) {
    sin->sin6_addr.__u6_addr.__u6_addr8[i] = ipv6[i];
  }
  // IMPROVEMENT: add support for scope ID and Flow? We should at least set them to 0.
}

void host_id_set_addr_in6(host_id *id, struct sockaddr_in6 *sa_in6) {
  memcpy(&(id->addr),sa_in6,sizeof(struct sockaddr_in6));
}

struct sockaddr* host_id_get_addr(host_id *id) {
  return &(id->addr.sa);
}

// returns the number of bytes in the address
int host_id_get_addr_as_byte_array(host_id *id, unsigned char *buf, int buflen) {
  if (id->addr.sa.sa_family == AF_INET) {
    if (buflen<4) { 
      return 0;
    }
    // FIXME: network byte ordering
    struct sockaddr_in* sin = &(id->addr.sa_in);
    buf[0] = (sin->sin_addr.s_addr >> 24) & 0xFF;
    buf[1] = (sin->sin_addr.s_addr >> 16) & 0xFF;
    buf[2] = (sin->sin_addr.s_addr >>  8) & 0xFF;
    buf[3] = (sin->sin_addr.s_addr >>  0) & 0xFF;
    return 4; 
  } else if (id->addr.sa.sa_family == AF_INET6) {
    if (buflen<16) { 
      return 0;
    }
    // FIXME: network byte ordering
    struct sockaddr_in6* sin = &(id->addr.sa_in6);
    for (int i=0;i<16;i++) {
      buf[i] = sin->sin6_addr.__u6_addr.__u6_addr8[i];
    }
    return 16;
  }
  return 0;
}


