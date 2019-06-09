// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<unistd.h>
#include<ctype.h> // for isprint()
#include<stdlib.h>
#include<time.h>
#include<strings.h>
#include<errno.h>

#include"log.h"
#include"server.h"
#include"service_port_forward.h"
#include"service_http.h"
#include"service_socks.h"
#include"thread_local.h"
#include"proxy_instance.h"
#include"config_file.h"
#include"version.h"
#include"main_config.h"

// These pragma's are to squelch a warning. 
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
int call_daemon() {
  return daemon(0,0);
}
#pragma GCC diagnostic pop

int set_ulimit(new_limit) {
  struct rlimit limit; 
  limit.rlim_cur=new_limit;
  limit.rlim_max=new_limit;
  if (setrlimit(RLIMIT_NOFILE, &limit) != 0) {
    printf("setrlimit() failed with errno=%d\n", errno);
    return 1;
  }
  return 0;
}

// main() is mostly concerned with init and configuraiton. 
int main(int argc,char **argv) {
  int rc;
  int help=0;
  int error=0;
  int daemonize=0;

  rc=thread_local_init();
  if (rc != 0) {
    fprintf(stderr, "Error initializing main thread (%i): %s\n",rc,strerror(rc));
    return 1;
  }
  // main() thread-local setup
  thread_local_set_proxy_instance(NULL);
  thread_local_set_service(NULL);
  thread_local_set_client_connection(NULL);
  thread_local_set_ssh_tunnel(NULL);
  thread_local_set_log_config(NULL);

  log_init();
  log_file_init();
  // main() logging setup
  main_config main_conf;
  main_config_init(&main_conf);
  main_conf.log.level=LOG_LEVEL_INFO;
  thread_local_set_log_config(&main_conf.log);



  // List of all logfiles we write to, and a special one to hold default settings
  log_file* log_file_list = NULL;
  log_file* log_file_default = new_log_file(NULL);
  if (log_file_default == NULL) {
    unexpected_exit(16,"Cannot instantiate log_file");
  }

  // List of all configured proxy instances. Defining just one proxy instance will set this list to
  // non-null and we won't use proxy_instance_default (except as a template for new instances.)
  // If, after configuration, proxy_instance_list is still null, we'll use proxy_instance_default instead;
  // this preserves the old behavior in a backwards-compatible way; there was only one (implied) proxy
  // instance.
  proxy_instance* proxy_instance_list = NULL;

  // the default proxy instance holds our default settings and are copied to other instances when created. 
  proxy_instance* proxy_instance_default = new_proxy_instance();
  if (proxy_instance_default == NULL) {
    unexpected_exit(10,"Cannot instantiate default proxy instance");
  }
  // Setup proxy defaults
  strncpy(proxy_instance_default->name,"default",sizeof(proxy_instance_default->name));
  proxy_instance_default->log.level = LOG_LEVEL_INFO;

  // List of SSH tunnels that have been provied/configuration
  ssh_tunnel* ssh_tunnel_list = ssh_tunnel_init(NULL);
  ssh_tunnel* ssh_tunnel_default = new_ssh_tunnel();
  if (ssh_tunnel_default == NULL) {
    unexpected_exit(15,"Cannot instantiate default ssh_tunnel");
  }

  srand(time(NULL));

  #define CONFIG_FILENAME_STACK_SIZE 200 // arbitrarily picked.
  char* filename_stack[CONFIG_FILENAME_STACK_SIZE+1];
  char option;
  while ((option = getopt(argc,argv, "c:dv:V:h")) != -1) {
    switch(option) {
      case 'c':
        if (!config_file_parse(&log_file_list, log_file_default, &main_conf, 
                               &proxy_instance_list, proxy_instance_default, 
                               &ssh_tunnel_list, ssh_tunnel_default, 
                               optarg, filename_stack, CONFIG_FILENAME_STACK_SIZE, 0)) {
          exit(1);
        }
        break;
      case 'd':
        daemonize = 1;
        break;
      case 'v':
        main_conf.log.level = log_level_from_str(optarg);
        trace("set main thread verbosity to %s",log_level_str(main_conf.log.level));
        break;
      case 'V':
        main_conf.log.file = find_or_create_log_file(&log_file_list, log_file_default, optarg);
        break;
      case 'h':
        help=1;
        break;
      case '?':
        if (optopt == 'p') {
          fprintf(stderr, "Option -%c requires an argument\n",optopt);
        } else if (isprint(optopt)) {
	  fprintf(stderr, "Unknown option '-%c'\n",optopt);
	}  else {
	  fprintf(stderr, "Unknown option character '0x%02x'\n",(int)optopt);
	}
	return 1;
      default:
        fprintf(stderr, "Unknown option '%c'\n",option);
        error=1;
        break;
    }
  }

  int index;
  for (index=optind; index < argc; index++) {
    printf("unused non-option argument: %s\n",argv[index]);
  }

  if (help) {
    printf("USAGE: %s <options>\n",argv[0]);
    printf("Version  %s  %s\n", SMARTSOCKSPROXY_VERSION, SMARTSOCKSPROXY_BUILD_DATE);
    printf("Options:\n");
    printf("  -c <file>    Load configuration from <file>. Can be used multiple times.\n");
    printf("  -d           Daemonize; start proxy in background.\n");
    printf("  -h           Print this help.\n");
    printf("  -v <level>   Set *main thread* verbosity to <level> where <level> is one of the following:\n");
    printf("                  error\n");
    printf("                  warn\n");
    printf("                  info\n");
    printf("                  debug\n");
    printf("                  trace\n");
    printf("                  trace2\n");
    printf("              The main thread includes config file parsing and various top-level functionality\n");
    printf("              To change the log level of proxy instances and SSH tunnels, set the debug level\n");
    printf("              in the corresponding section of the config file.\n");
    printf("  -V <file>   Set *main thread* log file to <file>. Other settings such as rotation policy\n");
    printf("              can be set in a \"logfile\" section of a config file. Set to \"-\" for STDOUT\n");
    printf("\n");

    return 2;
  }
  if (error) {
    return 1;
  }

  if (daemonize && call_daemon() < 0) {
    unexpected_exit(12,"daemon()");
  }

  if (set_ulimit(main_conf.ulimit) != 0) {
    unexpected_exit(13,"set_ulimit()");
  }

  return server(log_file_list, proxy_instance_list, ssh_tunnel_list, &main_conf);
}

