#ifndef PROXY_INSTANCE_H
#define PROXY_INSTANCE_H

#include"service.h"
#include"client_connection.h"
#include"log.h"

#define PROXY_INSTANCE_MAX_NAME_LEN  1024 
#define PROXY_INSTANCE_MAX_LISTENING_PORTS  200 // really, how many do you need?!

typedef struct proxy_instance {
  struct proxy_instance *next;

  char name[PROXY_INSTANCE_MAX_NAME_LEN];
  service *service_list;
  client_connection *client_connection_list;

  // each instance has a single logfile. Multiple instances can share a logfile
  // by using the same log_file_name; however, for shared logfiles it doesn't make
  // sense to use different size/rotate values, so we reserve the right to choose
  // randomly which ones to use.
  int  log_level;
  long log_max_size;        // maximum size of logfile before rotation
  int  log_max_rotate;      // maximum number of old logfiles we'll keep around
  char log_file_name[LOG_LOGFILE_NAME_MAX_LEN]; // filename. 
  log_info  *log;
 
} proxy_instance;

proxy_instance *new_proxy_instance();
proxy_instance *insert_proxy_instance(proxy_instance *head, proxy_instance *con);
proxy_instance *new_proxy_instance_from_template(proxy_instance *template);
char *proxy_instance_str(proxy_instance *inst, char *buf, int buflen);


#endif // PROXY_INSTANCE_H
