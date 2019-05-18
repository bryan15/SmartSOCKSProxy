// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<errno.h>
#include<strings.h>

#include"log.h"
#include"safe_blocking_readwrite.h"
#include"socks5.h"
#include"client_connection.h"
#include"shuttle.h"

int sock5_client_negotiate_auth(client_connection *con, int *failure_type) {
  unsigned char buf[300];
  int rc;

  buf[0]=0x05; // socks version 5
  buf[1]=0x01; // only one auth method supported by this client
  buf[2]=SOCKS5_AUTH_NONE_REQUIRED;
  rc=sb_write_len(con->fd_out, buf, 3);
  if (rc < 0) { return 0; }

  rc=sb_read_len(con->fd_out,buf,2);
  if (rc < 0) { return 0; }
  if (buf[0] != 0x05 || buf[1] != SOCKS5_AUTH_NONE_REQUIRED) {
    error("ssh SOCKS5 server does not support a suitable authentication method.");
    return 0;
  }
  trace("successfully authenticated with SSH SOCKS5 server");
  return 1;
}

int socks5_client_send_command(client_connection *con, int *failure_type) {
  unsigned char buf[300];
  int rc;

  buf[0]=0x05; // socks version 5
  buf[1]=con->socks_command;
  buf[2]=0x00; // reserved
  buf[3]=con->socks_address_type;
  int idx=4;
  if (con->socks_address_type == SOCKS5_ADDRTYPE_IPV4) {
    host_id_get_addr_as_byte_array(&con->dst_host,&(buf[idx]),4);
    idx+=4;
  } else if (con->socks_address_type == SOCKS5_ADDRTYPE_IPV6) {
    host_id_get_addr_as_byte_array(&con->dst_host,&(buf[idx]),16);
    idx+=16;
  } else if (con->socks_address_type == SOCKS5_ADDRTYPE_DOMAIN) {
    char *name = host_id_get_name(&con->dst_host);
    int len=strlen(name);
    buf[idx]=len; 
    ++idx;
    memcpy(&(buf[idx]),name,len);
    idx+=len;
  } else {
    error("Unknown address type %i",con->socks_address_type);
    return 0;
  }
  int port = host_id_get_port(&con->dst_host);
  buf[idx] = (port >> 8) & 0x0FF; ++idx; 
  buf[idx] = (port     ) & 0x0FF; ++idx; 
 
  rc=sb_write_len(con->fd_out, buf, idx);
  if (rc < 0) { return 0; }
  
  // TODO FIXME better debug on this socks5 client connection - include info from con 
  trace("sent command to SSH SOCKS5 server: %s", client_connection_str(con, (char*)buf, sizeof(buf)));
  return 1;
}


int socks5_client_get_command_response(client_connection *con, int *failure_type) {
  unsigned char buf[300];
  int rc;
  int idx;

  rc=sb_read_len(con->fd_out,buf,4);
  if (rc != 4) {
    if (rc == 0) { // socket unexpectedly closed
      *failure_type=SOCKS5_REPLY_CONNECTION_REFUSED;
      set_client_connection_status(con, CCSTATUS_ERR_NETWORK,"Connection Refused","Connection to remote host was refused. But SSH appears to be working fine.");
    }
    return 0;
  }
  idx=4;
  if (buf[0] != 0x05) { 
    char errStr[200];
    snprintf(errStr,sizeof(errStr)-1,"SSH SOCKS5 server replied with unsupported version %i",buf[0]);
    error(errStr);
    set_client_connection_status(con, CCSTATUS_ERR_INTERNAL,"Unexpected Error",errStr);
    return 0;
  }
  *failure_type=buf[1];
  int addr_type=buf[3];
  if (addr_type == SOCKS5_ADDRTYPE_IPV4) {
    rc=sb_read_len(con->fd_out, &(buf[idx]),4);
    if (rc < 0) { return 0;}
    idx += 4;
  } else if (addr_type == SOCKS5_ADDRTYPE_IPV6) {
    rc=sb_read_len(con->fd_out, &(buf[idx]),16);
    if (rc < 0) { return 0;}
    idx += 16;
  } else if (addr_type == SOCKS5_ADDRTYPE_IPV4) {
    int len=buf[4];
    idx += 1;
    rc=sb_read_len(con->fd_out, &(buf[idx]),len);
    if (rc < 0) { return 0;}
    idx += len;
  } else {
    error("Unknown address type %i",addr_type);
    return 0;
  }
  // port
  rc=sb_read_len(con->fd_out, &(buf[idx]),2);
  if (rc < 0) { return 0;}
  idx += 2;

  // I could check or log the IP and port, 
  // or I could ignore what the server sent back to us...

  if (buf[1] != SOCKS5_REPLY_SUCCEED) {
    trace("SSH SOCKS5 server returned error: %i",buf[1]);
    return 0;
  }
  trace("Successfully received response from SSH SOCKS5 server.");
  return 1;
}

// returns 1 on success, 0 on error
int connect_to_ssh_socks5_proxy(client_connection *con, ssh_tunnel *tunnel) {
  struct sockaddr_in saddr_in;
  int continue_connection_attempt=1;
  int connect_attempt=0;

  while (continue_connection_attempt) {
    ++connect_attempt;

    con->fd_out = socket(PF_INET, SOCK_STREAM, 0);
    if (con->fd_out < 0) {
      errorNum("socket()");
      return 0;
    }

    // FIXME get local address from ssh_tunnel structure
    bzero(&saddr_in, sizeof(saddr_in));
    saddr_in.sin_addr.s_addr = htonl(0x7F000001); // 127.0.0.1
    saddr_in.sin_family = AF_INET;

    /*
    route_rule* rule = decide_applicable_rule(proxy, srv, con);
    if (rule == NULL) {
    } 
    */
    // FIXME FIXME FIXME FIXME ROUTING SHIT GOES HERE
    /*
    if (con->route == SOME_ROUTE_PROXY_LAB) {
      saddr_in.sin_port = htons(31000); // viclabbas01
    } else if (connect_attempt & 0x10) {
      saddr_in.sin_port = htons(31002); // calbas01
    } else {
      saddr_in.sin_port = htons(31001); // vicbas01
    }
    */

    int rc;
    do {
      rc = connect(con->fd_out, (struct sockaddr*)&saddr_in, sizeof(saddr_in));
    } while (rc < 0 && (errno == EAGAIN || errno == EINTR));
    if (rc < 0) {
      int tmp_errno=errno;
      close(con->fd_out);
      errno=tmp_errno;
      con->fd_out=-1;
      if (errno == ECONNREFUSED) {
        set_client_connection_status(con, CCSTATUS_OKAY,"Waiting","SSH SOCKS5 proxy connection refused. Will try again in a few milliseconds.");
        trace("connect(ssh_proxy) connection refused. Will try again in a few milliseconds.");
        usleep(100000);
      } else {
        errorNum("connect(ssh_proxy)");
        set_client_connection_status(con, CCSTATUS_ERROR,NULL,strerror(tmp_errno));
        return 0;
      }
    } else {
      // successful connection
      continue_connection_attempt=0;
    }
    if (continue_connection_attempt && connect_attempt > 100) {
      set_client_connection_status(con, CCSTATUS_ERROR,"Timeout","Gave up waiting for SSH SOCKS5 proxy to become available. Does your SSH work?");
      return 0;
    }
  }
  return 1;
}

// returns 1 if connection successful, 0 otherwise.
// specific failure type written to failure_type
int connect_via_ssh_socks5(client_connection *con, ssh_tunnel *tunnel, int *failure_type) {
  int ok=1;

  *failure_type=SOCKS5_REPLY_SERVER_FAILURE;

  if (ok) {
    set_client_connection_status(con, CCSTATUS_OKAY,"Connecting","Connecting to SSH SOCKS5 server");
    ok = connect_to_ssh_socks5_proxy(con, tunnel);
  }
  if (ok) {
    set_client_connection_status(con, CCSTATUS_OKAY,"Negotiating","Negotiating connection with SSH SOCKS5 server");
    ok = sock5_client_negotiate_auth(con, failure_type);
  }
  if (ok) {
    ok = socks5_client_send_command(con, failure_type); 
  }
  if (ok) {
    ok = socks5_client_get_command_response(con, failure_type); 
  }
  if (ok) {
    set_client_connection_status(con, CCSTATUS_OKAY,NULL,NULL);
  }
  return ok; 
}

