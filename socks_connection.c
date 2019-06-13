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
#include"thread_msg.h"
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
  } while (rc < 0 && errno == EINTR);
  if (rc < 0) {
    int tmp_errno=errno;
    do {
      rc = close(con->fd_out);
    } while (rc<0 && errno == EINTR);
    con->fd_out=-1;
    errno=tmp_errno;
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
  route_rule *route = con->route;

  // Count how many tunnels this rule has.
  // Also notify main thread if we need SSH child activity. 
  int tun_max;
  int have_ssh_tunnel=0;
  for (tun_max=0; route->tunnel[tun_max] != NULL && tun_max < ROUTE_RULE_MAX_SSH_TUNNELS_PER_RULE; tun_max++) {
    ssh_tunnel *tun = route->tunnel[tun_max];
    if (tun != ssh_tunnel_direct && tun != ssh_tunnel_null && tun->command_to_run[0]) {
      have_ssh_tunnel=1;
    }
  }
  if (tun_max == 0) {
    error("Rule %s line %i used for routing, but it has no tunnels! This should not happen.", con->route->file_name, con->route->file_line_number);
    ok=0;  // no tunnels to service this connection!
  }
  if (ok && have_ssh_tunnel) {
    thread_msg_send(" ",1);
  } 

  int attempt_to_connect=1;
  int connect_attempt=0;
  while (ok && attempt_to_connect) {
    int tun_idx = connect_attempt % tun_max;
    ssh_tunnel *tun = route->tunnel[tun_idx];

    lock_client_connection(con);
    con->tunnel = tun;
    unlock_client_connection(con);

    int connection_created=0; 
    if (tun == ssh_tunnel_direct) {
      debug("Attempting to connect directly (%s).", tun->name);
      connection_created = connect_direct(con, failure_type);
    } else if (tun ==  ssh_tunnel_null) {
      debug("Connecting to null, the consumer of bytes, oblivion.");
      connection_created = connect_null(con, failure_type);
    // TODO: add support for connecting to a SOCKS5 server. IE: not direct or SSH.
    } else {
      debug("Attempting to connect to ssh_tunnel %s on port %i",tun->name, tun->socks_port);
      connection_created = connect_via_ssh_socks5(con, tun, failure_type);
    }
  
    if (connection_created) { 
      attempt_to_connect=0;
    } else { 
      lock_client_connection(con);
      con->tunnel = NULL;
      unlock_client_connection(con);
      if (con->fd_out >0) {
        do {
          rc = close(con->fd_out);
        } while (rc<0 && errno == EINTR);
        con->fd_out=-1;
      }
      if (connect_attempt >= 100) {
        trace("Connection attempt failed. Giving up.");
        ok=0;
      } else {
        debug("Connection attempt failed. Will try next ssh_tunnel in a few milliseconds.");
        usleep(100000);
      }
    }
    connect_attempt++;
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


