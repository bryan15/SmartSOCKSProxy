// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>

#include"log.h"
#include"host_id.h"

#include<stdio.h>
int resolve_dns_for_host_id(host_id *id) {
  if (!host_id_has_name(id)) {
    return 0;
  }

  trace("DNS resolution for %s",host_id_get_name(id));

  struct addrinfo *info_p;
  int result;

  do {
    result=getaddrinfo(host_id_get_name(id),NULL,NULL,&info_p);
  } while (result == EAI_AGAIN);

  // DNS resolution failed.
  if (result) { 
    warn("getaddrinfo(%s) %s",host_id_get_name(id), gai_strerror(result));
    return 0;
  }

  // DNS resolution succeeded. Pick one of the returned addresses.
  struct addrinfo *ai; 
  char hostname[256];
  int num_results=0;
  for ( ai=info_p ; ai!=NULL ; ai=ai->ai_next ) {
    num_results++;
    getnameinfo(ai->ai_addr, ai->ai_addrlen, hostname, sizeof(hostname), NULL, 0, NI_NUMERICHOST);
    debug("%s = %s", host_id_get_name(id), hostname);
  }

  // TODO: favor IPv4 results, fall back to IPv6 if supported
  // TODO: actually, we could try connecting to all of the results in sequence and return the first one that works. 
  // int my_result = rand() % num_results;
  int my_result = 0; // always pick first, to avoid IPv6 lower down in the options
  int i;
  for ( i=0, ai=info_p ; i<my_result ; ai=ai->ai_next, i++ );

  getnameinfo(ai->ai_addr, ai->ai_addrlen, hostname, sizeof(hostname), NULL, 0, NI_NUMERICHOST);
  debug("Selected IP address: %s = %s", host_id_get_name(id), hostname);

  struct sockaddr_in sin;
  sin.sin_len = sizeof(struct sockaddr_in);
  sin.sin_family = AF_INET;
  sin.sin_port=htons(host_id_get_port(id));
  inet_pton(AF_INET, hostname, &sin.sin_addr);
  host_id_set_addr_in(id,&sin);
  
  return 1;
}

