// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef SERVICE_H
#define SERVICE_H

// Anything with a listening socket which accepts TCP connections is a "service".
// Each service should be a sub-class of the "struct service" below.

#define SERVICE_TYPE_NONE          0 
#define SERVICE_TYPE_SOCKS         1
#define SERVICE_TYPE_PORT_FORWARD  2
#define SERVICE_TYPE_HTTP          3

// Okay, we need to have a conversation about polymorphism in C. 
// 
// The language doesn't support subclasses, polymorphism, or member functions. 
// 
// But there's a trick we can use to achieve the same net effect.
// Given "struct service" below, if we define a sub-class thus:
//   struct myStruct {
//      struct service srv;
//      int xyz;
//      ...
//      }
// 
// Then if we create an instance of myStruct: 
//   struct myStruct blah;
// 
// The following is true: 
//   &blah == &(blah.srv)
// 
// AS LONG AS "struct service srv" is the **first** member of myStruct, 
// the compiler will locate it first in RAM. Which explains why the above
// holds true. IE: The pointer to blah and the pointer to blah.srv are the same value. 
// 
// Therefore, we can cast a pointer to myStruct (&blah) to "struct service" and use
// it like a regular old "struct service" object. Voila, polymorphism in C. 
// IE: We can do this: 
//   struct myStruct my;
// 
//   struct service *srvPtr = (struct service*) &my;
//   srvPtr->port = 2000;
// 
//   struct myStruct *myPtr = (struct myStruct*) srvPtr;
//   myPtr->xyz = 5;
// 
// We use this (ugly) trick to create a parent class "struct service" 
// and then create subclasses "struct http", "struct port_forward", "struct socks_server" or whatever. 
// 
// I apologize, dear reader, for using this ugly trick. But, it lets us use OOP
// in C including inheritance and polymorphism, which is the goal here. 
// Old school....

typedef struct service {
  unsigned long long id;
  struct service *next;
  int type;                // SERVICE_TYPE_*
  char bind_address[300];  // local address we bound to
  int port;                // local port we listen on
  int fd;                  // file descriptor for listening socket

  // virtual functions which may be implemented by child classes
  char* (*str)(struct service *srv, char *buf, int buflen); // optional
  void* (*connection_handler)(void *data);                  // required if you want to do anything useful
} service;

service *new_service(int size, int type);
service *insert_service(service *head, service *srv);
char *service_default_str(service *srv, char *buf, int buflen);
void service_listen(service *srv);

#endif // SERVICE_H
