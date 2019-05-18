#include<stdio.h>
#include<errno.h>
#include<strings.h>
#include<stdlib.h>

#include"client_connection.h"
#include"service_socks.h"
#include"log.h"
#include"socks4.h"
#include"socks5.h"
#include"safe_blocking_readwrite.h"
#include"shuttle.h"
#include"socks5_client.h"
#include"thread_local.h"
#include"safe_close.h"
#include"string2.h"
#include"service_thread.h"
#include"socks_connection.h"
#include"route_rule.h"
#include"route_rules_engine.h"

//////////////////////////////////////////////////////////

int socks_get_client_version(client_connection *con) {
  unsigned char buf[10];
  int rc;

  rc=sb_read_len(con->fd_in,buf,1); if (rc != 1) return SOCKS_VERSION_ERROR;

  int socks_version = buf[0];
  trace("Client request SOCKS v 0x%02x",socks_version);
  if (socks_version != 0x04 && socks_version != 0x05) { 
    trace("request version not 4/5; closing connection.");
    buf[0]=0x05; 
    buf[1]=SOCKS5_AUTH_NO_ACCEPTABLE_METHODS; // no acceptable auth methods. Go away.
    sb_write_len(con->fd_in,buf,2);
    return SOCKS_VERSION_ERROR;
  }
  return socks_version;
}

int socks5_negotiate_auth(client_connection *con) {
  unsigned char buf[300];
  int rc;

  // auth methods
  unsigned int count;
  rc=sb_read_len(con->fd_in,buf,1); if (rc != 1) return SOCKS_AUTH_ERROR;
  count = buf[0];
  count &= 0x0FF;
  trace("Client provided 0x%02x available auth methods ",buf[0],count);

  // methods
  rc=sb_read_len(con->fd_in,buf,count); if (rc != count) return SOCKS_AUTH_ERROR;
  // read all the methods. We don't care about them. 
  for (int i=0;i<count;i++) {
    trace("  0x%02x",buf[i]);
  }

  // send auth method to use
  buf[0]=0x05; 
  buf[1]=SOCKS5_AUTH_NONE_REQUIRED; // we always pick no auth
  rc=sb_write_len(con->fd_in,buf,2); if (rc != 2) return SOCKS_AUTH_ERROR;
  trace("Send choosen auth type: NONE_REQUIRED");
  return SOCKS5_AUTH_NONE_REQUIRED;

}

//////////////////////////////////////////////////////////

int socks4_get_command(client_connection *con) {
  unsigned char buf[300];
  int rc;

  // command
  rc=sb_read_len(con->fd_in,buf,1); if (rc != 1) return SOCKS5_CMD_ERROR;
  unsigned char command=buf[0];
  if (command != SOCKS4_CMD_CONNECT) return SOCKS5_CMD_ERROR;

  // destination port
  rc=sb_read_len(con->fd_in,buf,2); if (rc != 2) return SOCKS5_CMD_ERROR;
  int port = ((unsigned int)buf[0]) << 8 | (unsigned int)buf[1];
  trace("  port %i", port);

  // destination IPv4 address
  unsigned char ipv4[4];
  rc=sb_read_len(con->fd_in,ipv4,4); if (rc != 4) return SOCKS5_CMD_ERROR;
  trace("  address IPv4: %02x %02x %02x %02x", 
    ipv4[0],
    ipv4[1],
    ipv4[2],
    ipv4[3]
   );

  lock_client_connection(con); 
  con->socks_version               = 4;
  con->socks_command               = command; 
  con->socks_command_original      = command;      // save original unmolested by processing or routing
  con->socks_address_type          = SOCKS5_ADDRTYPE_IPV4;
  con->socks_address_type_original = SOCKS5_ADDRTYPE_IPV4; // save original unmolested by processing or routing
  host_id_set_addr_in_from_byte_array(&con->dst_host,ipv4,port);
  con->dst_host_original=con->dst_host; // save original unmolested by processing or routing
  unlock_client_connection(con); 
 
  // userid
  int max=1000;
  do {
    rc=sb_read_len(con->fd_in,buf,1); if (rc != 1) return SOCKS5_CMD_ERROR;
  } while (buf[0] != 0 && --max > 0);
  if (max <= 0)  return SOCKS5_CMD_ERROR;

  return SOCKS4_CMD_CONNECT;
}

//////////////////////////////////////////////////////////

int socks5_get_command(client_connection *con) {
  unsigned char buf[300];
  int rc;

  // version and command
  rc=sb_read_len(con->fd_in,buf,4); if (rc != 4) return SOCKS5_CMD_ERROR; 
  unsigned char version=buf[0];
  unsigned char command=buf[1];
  unsigned char rsv=buf[2];
  unsigned char address_type=buf[3];
  trace("Client request version 0x%02x, command 0x%02x, address type 0x%02x",version, command, address_type);
  if (version != 0x05) return SOCKS5_CMD_ERROR;
  if (rsv != 0x00) return SOCKS5_CMD_ERROR;


  unsigned char addr[4096];
  if (address_type == SOCKS5_ADDRTYPE_IPV4) { // IPv4
    rc=sb_read_len(con->fd_in,addr,4); if (rc != 4) return SOCKS5_CMD_ERROR;
    trace("  address IPv4: %02x %02x %02x %02x", 
      addr[0],
      addr[1],
      addr[2],
      addr[3]
      );
  } else if (address_type == SOCKS5_ADDRTYPE_IPV6) { // IPv6
    rc=sb_read_len(con->fd_in,addr,16); if (rc != 16) return SOCKS5_CMD_ERROR;
    trace("  address IPv6");
    return SOCKS5_CMD_ERROR; // we don't support IPv6
  } else if (address_type == SOCKS5_ADDRTYPE_DOMAIN) { // DNS
    // length
    rc=sb_read_len(con->fd_in,buf,1); if (rc != 1) return SOCKS5_CMD_ERROR;
    unsigned int length = (unsigned int)buf[0] & 0x0FF;
    // data
    rc=sb_read_len(con->fd_in,addr,length); if (rc != length) return SOCKS5_CMD_ERROR;
    addr[length]=0; // add null terminator
    trace("  address DNS: %s",addr);
  } else { 
    return SOCKS5_CMD_ERROR; 
  }
  
  // destination port
  rc=sb_read_len(con->fd_in,buf,2); if (rc != 2) return SOCKS5_CMD_ERROR;
  int port = ((unsigned int)buf[0]) << 8 | (unsigned int)buf[1];
  trace("  port %i", port);

  lock_client_connection(con);
  con->socks_version               = version;
  con->socks_command               = command; 
  con->socks_command_original      = command;      // save original unmolested by processing or routing
  con->socks_address_type          = address_type;
  con->socks_address_type_original = address_type; // save original unmolested by processing or routing
  if (address_type == SOCKS5_ADDRTYPE_IPV4) { 
    host_id_set_addr_in_from_byte_array(&con->dst_host,addr,port);
  } else if (address_type == SOCKS5_ADDRTYPE_IPV6) { // IPv6
    host_id_set_addr_in6_from_byte_array(&con->dst_host,addr,port);
  } else if (address_type == SOCKS5_ADDRTYPE_DOMAIN) { // DNS
    host_id_set_name(&con->dst_host,(char*)addr);
  }
  con->dst_host_original = con->dst_host;
  unlock_client_connection(con);

  if (con->socks_command == SOCKS5_CMD_CONNECT) {
    return SOCKS5_CMD_CONNECT; 
  } else if (con->socks_command == SOCKS5_CMD_BIND) {
    return SOCKS5_CMD_BIND; 
  } else if (con->socks_command == SOCKS5_CMD_UDP_ASSOCIATE) {
    return SOCKS5_CMD_UDP_ASSOCIATE; 
  }
  return SOCKS5_CMD_ERROR;  

}

int socks4_respond(client_connection *con, int failure_type) {
  unsigned char buf[300];
  int idx;
  int rc;

  buf[0]=0;
  if (failure_type == SOCKS5_REPLY_SUCCEED) {
    buf[1]=SOCKS4_CD_REQUEST_GRANTED;
  } else {
    buf[1]=SOCKS4_CD_REQUEST_REJECTED_OR_FAILED;
  }
  buf[2]=0; // dst port
  buf[3]=0; // dst port
  buf[4]=0; // dst ip
  buf[5]=0; // dst ip
  buf[6]=0; // dst ip
  buf[7]=0; // dst ip
  idx = 8;

  rc = sb_write_len(con->fd_in,buf,idx);
  if (rc != idx) {
    error("Error sending reply; %i bytes",idx);
    return 0;
  } 
  trace("reply to client; %i bytes",idx);
  return 1;
}

// The original version of this routine conforms to RFC1928, but Java's SocksSocketImpl 
// has a bug when processing DNS replies. In java.net.SocksSocketImpl.connect() line 522
// 
//            case DOMAIN_NAME:
//                len = data[1];
//                byte[] host = new byte[len];
//                i = readSocksReply(in, host, deadlineMillis);
//                if (i != len)
//                    throw new SocketException("Reply from SOCKS server badly formatted");
//                data = new byte[2];
//                i = readSocksReply(in, data, deadlineMillis);
//                if (i != 2)
//                    throw new SocketException("Reply from SOCKS server badly formatted");
//                break;
//
// The 'len = data[1]' is completely and absolutely wrong. data[1] contains
// the status of the connection setup attempt. In the case of success, data[1] = 0. 
// data[3] contains the address type in the response, in this case DOMAIN_NAME. 
// Which explains why we're looking at this chunk of code. For DOMAIN_NAME, 
// the first byte (which has not been read yet) is the length, followed by a string
// (not 0-terminated). 
// In other words, the code should look like this:
//            case DOMAIN_NAME:
//                byte[] lenBuf = new byte[1];
//                i = readSocksReply(in, lenBuf, deadlineMillis);
//                if (i != 1)
//                    throw new SocketException("Reply from SOCKS server badly formatted");
//                len = lenBuf[0]
//                byte[] host = new byte[len];
//                i = readSocksReply(in, host, deadlineMillis);
//                if (i != len)
//                    throw new SocketException("Reply from SOCKS server badly formatted");
//                data = new byte[2];
//                i = readSocksReply(in, data, deadlineMillis);
//                if (i != 2)
//                    throw new SocketException("Reply from SOCKS server badly formatted");
//                break;
// 
// I'll point out the IPv6 handling is similarly borked. This code was obviously never tested. 
//
// To avoid triggering this bug, we do what SSH does -- never return a DNS value. 
// Instead, just return an IPv4 and port set to all 0's. Java's IPv4 handling is correct.
// 
// Found it: https://bugs.java.com/bugdatabase/view_bug.do?bug_id=8162760
// Fixed in JDK 9
//
// SOCKS5 replies, 1 byte each unless noted:
//    version
//    reply status
//    0x00
//    address type, value = 1(ipv4), 3(dns), 4(ipv6)
//    bind_address <variable size>
//    port <2-bytes>
// returns 1 if succeeds in writing reply, 0 otherwise.
int socks5_respond(client_connection *con, int failure_type) {
  unsigned char buf[300];
  int idx;
  int rc;

  buf[0]=0x05; // SOCKS5 version
  buf[1]=(unsigned char)failure_type;
  buf[2]=0x00; // reserved
  buf[3]=SOCKS5_ADDRTYPE_IPV4;

  buf[4]=0;
  buf[5]=0;
  buf[6]=0;
  buf[7]=0;

  buf[8]=0;
  buf[9]=0;
  idx=10;

  rc = sb_write_len(con->fd_in,buf,idx);
  if (rc != idx) {
    error("Error sending reply; %i bytes",idx);
    return 0;
  } 
  trace("reply to client; %i bytes",idx);
  return 1;
}

// returns 1 if succeeds in writing reply, 0 otherwise.
int socks5_respond2(client_connection *con, int failure_type) {
  unsigned char buf[300];
  int idx;
  int rc;

  buf[0]=0x05; // SOCKS5 version
  buf[1]=(unsigned char)failure_type;
  buf[2]=0x00; // reserved
  buf[3]=con->socks_address_type;
  idx=4;
  if (con->socks_address_type == SOCKS5_ADDRTYPE_IPV4) {
    host_id_get_addr_as_byte_array(&con->dst_host,&(buf[idx]),4);
  } else if (con->socks_address_type == SOCKS5_ADDRTYPE_DOMAIN) {
    char *name = host_id_get_name(&con->dst_host);
    int len=strlen(name);
    buf[4]=len;
    strncpy((char *)&(buf[5]),name,sizeof(buf)-idx-1);
    idx += len + 1; // + 1 for null-terminator
  } else if (con->socks_address_type == SOCKS5_ADDRTYPE_IPV6) {
    host_id_get_addr_as_byte_array(&con->dst_host,&(buf[idx]),16);
    idx += 16;
  } else {
    buf[3]=SOCKS5_ADDRTYPE_IPV4;
    buf[4]=buf[5]=buf[6]=buf[7]=0;
    idx=8;
  }
  int port = host_id_get_port(&con->dst_host);
  buf[idx]=(port >> 8) & 0x0FF; ++idx;
  buf[idx]=(port     ) & 0x0FF; ++idx;

  rc = sb_write_len(con->fd_in,buf,idx);
  if (rc != idx) {
    error("Error sending reply; %i bytes",idx);
    return 0;
  } 
  trace("reply to client; %i bytes",idx);
  return 1;
}

int socks_respond(client_connection *con, int failure_type) {
  if (con->socks_version == 4) {
    return socks4_respond(con,failure_type);
  } 
  return socks5_respond(con,failure_type);
} 

//////////////////////////////////////////////////////////

// returns void* because its started as a new thread, and that's what
// the interface expects. 
void *service_socks_connection_handler(void *data) {
  service_thread_setup(data);
  thread_data *tdata = data;
  proxy_instance *proxy = tdata->proxy;
  service_socks *socks=(service_socks*)tdata->srv;
  client_connection *con = tdata->con;
  free(data);  // this is our responsibility.

  lock_client_connection(con);
  con->socks_version = SOCKS_VERSION_ERROR;
  con->socks_command = con->socks_command_original = SOCKS5_CMD_ERROR;
  host_id_set_port(&con->dst_host,0); 
  unlock_client_connection(con);

  int ok=1;

  if (ok) {
    int socks_version = socks_get_client_version(con);
    lock_client_connection(con);
    con->socks_version = socks_version;
    unlock_client_connection(con);
    if (con->socks_version == SOCKS_VERSION_ERROR) {
      ok=0;
    }
  }

  if (ok && con->socks_version == 5) {
    int auth_type;
    auth_type = socks5_negotiate_auth(con);
    if (auth_type == SOCKS5_AUTH_NONE_REQUIRED) {
      trace("Auth: None");
    } else if (auth_type == SOCKS5_AUTH_USERNAME_PASSWORD) {
      // see https://tools.ietf.org/html/rfc1929
      debug("Auth: username / password not supported. Closing connection.");
      ok=0;
    } else if (auth_type == SOCKS5_AUTH_GSSAPI) {
      debug("Auth: GSSAPI not supported. Closing connection.");
      ok=0;
    } else {
      debug("Auth: unknown or error. Closing connection.");
      ok=0;
    }
  }

  // we don't use command_type but, meh, maybe one day we will...
  if (ok) {
    int command_type;
    if (con->socks_version == 4) {
      command_type=socks4_get_command(con);
    } else {
      command_type=socks5_get_command(con);
    }
    if (command_type == SOCKS5_CMD_ERROR) {
      ok=0;
    }
  } 

  if (ok) {
    ok = decide_applicable_rule(proxy, (service*)socks, con);
  }

  int failure_type;
  if (ok) {
    ok=socks_connect(proxy, (service*)socks, con, &failure_type);
  }
  if (ok) {
    if (socks_respond(con, failure_type) != 1) {
      ok=0;
    }
  }
  if (ok) {
    ok = socks_connect_shuttle(con);
  }

  service_thread_shutdown(con, ok);
  return NULL;
}

//////////////////////////////////////////////////////////

char *service_socks_str(service_socks *socks, char *buf, int buflen) {
  return service_default_str((service*)socks,buf,buflen) ;
}

service_socks *new_service_socks() {
  service_socks *socks = (service_socks*) new_service(sizeof(struct service_socks), SERVICE_TYPE_SOCKS);
  socks->srv.str = (void*)&service_socks_str; // void* is to bypass incompatible pointer warning. See service.h for polymorphism in C.
  socks->srv.connection_handler = &service_socks_connection_handler;
  return socks;
}

service_socks *parse_service_socks_spec(char *strIn) {
  char *token;
  char *strPtr;
  char *local_copy;
  int okay = 1;
  service_socks *socks = new_service_socks();

  // a paremeter with 2 parts will have ':' 1 time in it. Hence the +1
  int parts = string_count_char_instances(strIn,':') + 1;
  if (parts < 1 || parts > 2) {
    error("SocksServer must have 1 or 2 parts separated by a colon: [bind_address:]local_port");
    error("The provided paramter \"%s\" has %i parts.",strIn,parts);
    error("Example:  127.0.0.1:123  or  123");
    return NULL;
  }
  trace2("SocksServer Parse: From \"%s\" got %i parts:", strIn, parts);

  // get a local copy which we can modify
  local_copy = strPtr = strdup(strIn);
  if (strPtr == NULL) {
    error("error parsing socks_server");
    okay = 0;
  }
  if (okay) {
    socks = new_service_socks();
  }

  // apply default to optional first part: the local address to bind to
  int index = 0;
  if (okay && parts == 3) {
    index++;
    strncpy(socks->srv.bind_address,"127.0.0.1",sizeof(socks->srv.bind_address)-1);
    trace2("part %i:  %s  (default)", index,socks->srv.bind_address);
  }
 
  while (okay && (token = strsep(&strPtr,":"))) {
    index++;
    trace2("part %i:  %s", index, token);
    switch(index) {
      case 1:
        strncpy(socks->srv.bind_address,token,sizeof(socks->srv.bind_address)-1);
        break;
      case 2:
        if (sscanf(token,"%i",&socks->srv.port) != 1) {
          error("Error converting \"%s\" to a port number. Input parameter: \"%s\".",token,strIn);
          okay=0;
        }
        break;
      default:
        error("error parsing socks server specification");
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
    if (socks != NULL) {
      free(socks);
      socks=NULL;
    }
  }

  return socks;
}

