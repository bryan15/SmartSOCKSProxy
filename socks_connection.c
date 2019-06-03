// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<errno.h>
#include<strings.h>
#include<stdlib.h>
#include <arpa/inet.h>

#include"log.h"
#include"socks4.h"
#include"socks5.h"
#include"client_connection.h"
#include"safe_blocking_readwrite.h"
#include"shuttle.h"
#include"socks5_client.h"
#include"thread_local.h"
#include"safe_close.h"
#include"string2.h"
#include"dns_util.h"

// returns 1 if connection successful, 0 otherwise.
int connect_null(client_connection *con, int *failure_type) {
  return 1;
}

// returns 1 if connection successful, 0 otherwise.
// specific failure type written to failure_type
int connect_direct(client_connection *con, int *failure_type) {
  *failure_type = SOCKS5_REPLY_SERVER_FAILURE;

  if (host_id_has_name(&con->dst_host) && !host_id_has_addr(&con->dst_host)) {
    // first, check if the name can be converted to an IP address. 
    // TODO: check for IPV6
    struct sockaddr_in sin; 
    sin.sin_len=sizeof(sin);
    sin.sin_family=AF_INET;
    sin.sin_port=htons(host_id_get_port(&con->dst_host));
    if (inet_pton(AF_INET,host_id_get_name(&con->dst_host),&sin.sin_addr) == 1) {
      host_id_set_addr_in(&con->dst_host,&sin);
      trace("%s converted to IPv4 address",host_id_get_name(&con->dst_host));
    } else {
      resolve_dns_for_host_id(&con->dst_host);
    }
  }

  struct sockaddr_in saddr_in;
  if (host_id_has_addr(&con->dst_host)) {
    if (con->dst_host.addr.sa.sa_family == AF_INET) {
      memcpy(&saddr_in,&(con->dst_host.addr.sa_in),sizeof(struct sockaddr_in));
    } else if (con->socks_address_type == SOCKS5_ADDRTYPE_IPV6) {
      warn("Unsupported address type: IPv6");
      *failure_type = SOCKS5_REPLY_ADDRESS_TYPE_NOT_SUPPORTED;
      return 0;
    } else {
      error("Unsupported address type: %u",con->socks_address_type);
      *failure_type = SOCKS5_REPLY_ADDRESS_TYPE_NOT_SUPPORTED;
      return 0;
    }
  } else {
    char buf[2048];
    error("Unsupported address and/or type: %s", host_id_str(&(con->dst_host),buf,sizeof(buf)));
    *failure_type = SOCKS5_REPLY_ADDRESS_TYPE_NOT_SUPPORTED;
    return 0;
  }

  con->fd_out = socket(PF_INET, SOCK_STREAM, 0);
  if (con->fd_out < 0) {
    errorNum("socket()");
    return 0;
  }

  int rc;
  do {
    rc = connect(con->fd_out, (struct sockaddr*)&saddr_in, sizeof(saddr_in));
  } while (rc < 0 && (errno == EAGAIN || errno == EINTR));
  if (rc < 0) {
     char buf[500];
     errorNum("connect()");
     error( "for connection %s",client_connection_str(con,buf, sizeof(buf)));
     if (errno == ECONNREFUSED || errno == ECONNRESET) {
       *failure_type = SOCKS5_REPLY_CONNECTION_REFUSED;
     }
     return 0;
  }

  trace("Connect success");
  *failure_type = SOCKS5_REPLY_SUCCEED;
  return 1;
}

int socks_connect(proxy_instance *proxy, service *srv, client_connection *con, int *failure_type) {
  int ok=1;
  int iteration=0;
  int do_loop=1;

  ssh_tunnel* tunnel = NULL;

  // count the number of available tunnels for this route. Basically, we treat one different from >one. 
  int tunnel_num;
  for (tunnel_num=0; con->route->tunnel[tunnel_num] != NULL ; tunnel_num++);
  if (tunnel_num == 0) {
    error("Rule %s line %i used for routing, but it has no tunnels! This should not happen.", con->route->file_name, con->route->file_line_number);
    ok=0;  // no tunnels to service this connection!
  }
  if (ok) {
    trace("%i tunnels available for this route. I'm going to pretend there's only one. FIXME!",tunnel_num);
    // FIXME tunnel handling
    tunnel = con->route->tunnel[0];
    con->tunnel = tunnel;
    if (tunnel == ssh_tunnel_direct) {
      ok = connect_direct(con, failure_type);
    } else if (tunnel ==  ssh_tunnel_null) {
      ok = connect_null(con, failure_type);
    // TODO: add support for connecting to a SOCKS5 server. IE: not direct or SSH.
    } else {
      ok = connect_via_ssh_socks5(con, tunnel, failure_type);
    }
  }
/* 
  while (ok && do_loop) {
    iteration++;
  // FIXME not sure if tunnel selection is right here.
    // try each 
    for (tunnel_num=0; con->route->tunnel[tunnel_num] != NULL; tunnel_num++) {
      tunnel = con->route->tunnel[tunnel_num];
      if (tunnel == ssh_tunnel_direct) {
        ok = connect_direct(con, failure_type);
      } else if (tunnel ==  ssh_tunnel_null) {
        ok = connect_null(con, failure_type);
      // TODO: add support for connecting to a SOCKS5 server. IE: not direct or SSH.
      } else {
        ok = connect_via_ssh_socks5(con, tunnel, failure_type);
      }
    }
  }
*/

// FIXME HERE 
 /*
  *failure_type = SOCKS5_REPLY_SERVER_FAILURE;
  if (con->rule == _direct) {
  } else if (con->tunnel == ssh_tunnel_null) {
    ok = connect_null(con, failure_type);
  } else if (con->tunnel != NULL) {
    ok = connect_via_ssh_socks5(con, failure_type);
  } else {
    error("Invalid route for connection");
    ok=0;
  }
 */
  if (ok && tunnel == NULL) { 
    ok=0;
  }
  return ok;
}

int socks_connect_shuttle(client_connection *con) {
  int ok=1;
  if (con->tunnel == ssh_tunnel_null) {
    ok = shuttle_null_connection(con);
  } else {
    ok = shuttle_data_back_and_forth(con);
  }
  return ok;
}

char *socks_connect_str_destination(client_connection *con, char *buf, int buflen) {
  char addr1[1024];
  char addr2[1024];
  host_id_str(&con->dst_host,addr1,sizeof(addr1));
  host_id_str(&con->dst_host,addr2,sizeof(addr2));
  if (strcmp(addr1,addr2)==0) {
    snprintf(buf,buflen,"%s",addr1);
  } else {
    snprintf(buf,buflen,"%s(changed to %s)",addr1,addr2);
  }
  return buf;
}


