// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<stdlib.h>
#include <arpa/inet.h>

#include"unit_test.h"
#include"host_id.h"



/*

struct sockaddr {
        __uint8_t       sa_len;         // total length 
        sa_family_t     sa_family;      // [XSI] address family 
        char            sa_data[14];    // [XSI] addr value (actually larger) 
};

struct sockaddr_in {
        __uint8_t       sin_len;
        sa_family_t     sin_family;
        in_port_t       sin_port;
        struct  in_addr sin_addr;
        char            sin_zero[8];
};
struct in_addr {
        in_addr_t s_addr;
};
typedef u_long in_addr_t

struct sockaddr_in6 {
        __uint8_t       sin6_len;       // length of this struct(sa_family_t)
        sa_family_t     sin6_family;    // AF_INET6 (sa_family_t)
        in_port_t       sin6_port;      // Transport layer port # (in_port_t) 
        __uint32_t      sin6_flowinfo;  // IP6 flow information 
        struct in6_addr sin6_addr;      // IP6 address 
        __uint32_t      sin6_scope_id;  // scope zone index 
};

typedef struct host_id {
  char name[MAX_DNS];
  union { // use a union so we don't have to know how big these structures are
    struct sockaddr     sa;
    struct sockaddr_in  sa_in;
    struct sockaddr_in6 sa_in6;
  } addr;
  int port; // duplication, unfortunately, but sometimes we don't have a valid addr to store this in.
} host_id;

*/

void unit_test_host_id() {
  host_id id_mem, *id;
  id=&id_mem;

  ut_name("host_id.host_id_init()");
  host_id_init(id);
  ut_assert("name", id->name[0]==0);
  ut_assert("port", id->port==0);
  ut_assert("addr", id->addr.sa.sa_family==AF_UNSPEC);
  ut_assert_false("host_id_has_name()", host_id_has_name(id));
  ut_assert_false("host_id_has_addr()", host_id_has_addr(id));

  char* name="test.com";
  ut_name("host_id name");
  host_id_init(id);
  host_id_set_name(id, name);
  ut_assert_true("host_id_has_name()", host_id_has_name(id));
  ut_assert_string_match("host_id_get_name()", name, host_id_get_name(id));
  ut_assert_string_match("id->name", name, id->name);

  int port=1234;
  ut_name("host_id port");
  host_id_init(id);
  host_id_set_port(id, port); 
  ut_assert_int_match("host_id_get_port()", port, host_id_get_port(id));
  ut_assert_int_match("id->port", port, id->port);
  ut_assert("addr", id->addr.sa.sa_family==AF_UNSPEC);

  ut_name("host_id port w/ addr");
  host_id_init(id);
  id->addr.sa_in.sin_family=AF_INET;
  host_id_set_port(id, port); 
  ut_assert_int_match("host_id_get_port()", port, host_id_get_port(id));
  ut_assert_int_match("id->port", port, id->port);
  ut_assert_int_match("id->addr.sin_addr.s_port", port, id->addr.sa_in.sin_port);
  
  char* ip_str="1.2.3.4"; 
  char  buf[1024];
  struct sockaddr_in sin; 
  ut_name("host_id addr");
  host_id_init(id);
  sin.sin_len = sizeof(sin);
  sin.sin_family = AF_INET;
  inet_pton(AF_INET,ip_str,&sin.sin_addr);
  ;sin.sin_port=port;
  host_id_set_addr_in(id, &sin);
  char *tmp=host_id_addr_str(id, buf, sizeof(buf));
  if (tmp == NULL) { 
    buf[0] = 0; 
  }
  ut_assert_string_match("host_id_set_addr_in() -> host_id_addr_str()", ip_str, buf);
  ut_assert_int_match("host_id_get_port()", port, host_id_get_port(id));
  ut_assert_int_match("id->port", port, id->port);
  ut_assert_int_match("id->addr.sin_addr.s_port", port, id->addr.sa_in.sin_port);

  ut_name("host_id addr byte array");
  unsigned char ubuf[40];
  ubuf[0]=1; 
  ubuf[1]=2; 
  ubuf[2]=3; 
  ubuf[3]=4; 
  host_id_init(id);
  host_id_set_addr_in_from_byte_array(id,ubuf,port);
  tmp=host_id_addr_str(id, buf, sizeof(buf));
  if (tmp == NULL) { 
    buf[0] = 0; 
  }
  ut_assert_string_match("host_id_set_addr_in_from_byte_array() -> host_id_addr_str()", ip_str, buf);
  ut_assert_int_match("port",port,host_id_get_port(id));

  ubuf[0]=ubuf[1]=ubuf[2]=ubuf[3]=0;
  ut_assert_int_match("host_id_get_addr_as_byte_array()",4,host_id_get_addr_as_byte_array(id,ubuf,sizeof(ubuf)));
  ut_assert_int_match("..0",1,ubuf[0]);
  ut_assert_int_match("..1",2,ubuf[1]);
  ut_assert_int_match("..2",3,ubuf[2]);
  ut_assert_int_match("..3",4,ubuf[3]);

}

