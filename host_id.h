// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef HOST_ID_H
#define HOST_ID_H

#include<netinet/in.h>

// https://www.ietf.org/rfc/rfc1035.txt 
// section 2.3.4 Size Limits
//   names 255 octets or less
#define MAX_DNS 260

typedef struct host_id {
  char name[MAX_DNS];
  union { // use a union so we don't have to know how big these structures are
    struct sockaddr     sa;
    struct sockaddr_in  sa_in;
    struct sockaddr_in6 sa_in6;
  } addr;
  int port; // duplication, unfortunately, but sometimes we don't have a valid addr to store this in.
} host_id;

void host_id_init(host_id *id);

char *host_id_addr_str(host_id *id, char *buf, int len);
char *host_id_str(host_id *id, char *buf, int len);

int host_id_has_name(host_id *id);
void host_id_set_name(host_id *id, char *name);
char* host_id_get_name(host_id *id);

void host_id_set_port(host_id *id, int port);
int host_id_get_port(host_id *id);

int host_id_has_addr(host_id *id);
void host_id_set_addr_in(host_id *id, struct sockaddr_in *sa_in);
void host_id_set_addr_in_from_byte_array(host_id *id, unsigned char* ipv4, int port);
void host_id_set_addr_in_port(host_id *id, int port);
void host_id_set_addr_in6(host_id *id, struct sockaddr_in6 *sa_in6);
void host_id_set_addr_in6_from_byte_array(host_id *id, unsigned char* ipv6, int port);
struct sockaddr* host_id_get_addr(host_id *id);
int host_id_get_addr_as_byte_array(host_id *id, unsigned char *buf, int buflen);

#endif // HOST_ID_H
