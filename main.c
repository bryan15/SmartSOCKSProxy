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

  log_init();
  rc=thread_local_init();
  if (rc != 0) {
    fprintf(stderr, "Error initializing main thread (%i): %s\n",rc,strerror(rc));
    return 1;
  }
  // main() thread-local setup
  thread_local_set_proxy_instance(NULL);
  thread_local_set_service(NULL);
  thread_local_set_client_connection(NULL);

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
  proxy_instance_default->log_max_size = (1024*1024) * 100;
  proxy_instance_default->log_max_rotate = 4;
  strncpy(proxy_instance_default->log_file_name,"/tmp/smartsocksproxy.log",sizeof(proxy_instance_default->log_file_name)-1);
  proxy_instance_default->log_file_name[sizeof(proxy_instance_default->log_file_name)-1]=0; // paranoia: no unterminated strings for us!
  strncpy(proxy_instance_default->name,"default",sizeof(proxy_instance_default->name));
  thread_local_set_proxy_instance(proxy_instance_default);

  // List of SSH tunnels that have been provied/configuration
  ssh_tunnel* ssh_tunnel_list = ssh_tunnel_init(NULL);
  ssh_tunnel* ssh_tunnel_default = new_ssh_tunnel();
  if (ssh_tunnel_default == NULL) {
    unexpected_exit(15,"Cannot instantiate default ssh_tunnel");
  }

  // Configuration via command-line is contextual. The following variable always
  // points to the object we're currently modifying.
  proxy_instance* proxy_instance_modify = proxy_instance_default;
  ssh_tunnel* ssh_tunnel_modify = ssh_tunnel_default;

  srand(time(NULL));

  char option;
  service_port_forward *port_forward_tmp=NULL;
  service_socks *socks_tmp=NULL;
  service_http *http_tmp=NULL;
  proxy_instance *proxy_instance_tmp=NULL;
  ssh_tunnel* ssh_tmp=NULL;
  while ((option = getopt(argc,argv, "c:dvV:hp:l:L:S:r:i:m:s:")) != -1) {
    switch(option) {
      case 'c':
        if (!config_file_parse(&proxy_instance_list, proxy_instance_default, &ssh_tunnel_list, ssh_tunnel_default, optarg)) {
          exit(1);
        }
        break;
      case 'd':
        daemonize = 1;
        break;
      case 'v':
        proxy_instance_modify->log_level--;
        if (proxy_instance_modify->log_level < LOG_TRACE2) {
          proxy_instance_modify->log_level = LOG_TRACE2;
        }
        break;
      case 'V':
        proxy_instance_modify->log_level = log_level_from_str(optarg);
        // info("Set verbosity level for proxy '%s' to %s (%i)",proxy_instance_modify->name, log_level_str(proxy_instance_modify->log_level), proxy_instance_modify->log_level);
        break;
      case 'h':
        help=1;
        break;
      case 'p':
        socks_tmp = parse_service_socks_spec(optarg); 
        if (socks_tmp == NULL) {
          error=1;
        } else {
          proxy_instance_modify->service_list = insert_service(proxy_instance_modify->service_list, (service*)socks_tmp);
        }
        socks_tmp=NULL;
        break;
      case 's':
        ssh_tmp = parse_ssh_tunnel_spec(optarg);
        if (ssh_tmp == NULL) {
          error=1;
        } else {
          ssh_tunnel_list = insert_ssh_tunnel(ssh_tunnel_list, ssh_tmp);
        } 
        ssh_tmp=NULL;
        break;
      case 'l':
        strncpy(proxy_instance_modify->log_file_name, optarg, LOG_LOGFILE_NAME_MAX_LEN-1);
        proxy_instance_modify->log_file_name[LOG_LOGFILE_NAME_MAX_LEN-1]=0; // ensure null terminated
        break;
      case 'S':
        sscanf(optarg, "%li",&(proxy_instance_modify->log_max_size));
        break;
      case 'L':
        port_forward_tmp = parse_service_port_forward_spec(optarg);
        if (port_forward_tmp == NULL) {
          error=1;
        } else {
          proxy_instance_modify->service_list = insert_service(proxy_instance_modify->service_list, (service*)port_forward_tmp);
        } 
        port_forward_tmp=NULL;
        break;
      case 'm':
        http_tmp = parse_service_http_spec(optarg);
        if (http_tmp == NULL) {
          error=1;
        } else {
          proxy_instance_modify->service_list = insert_service(proxy_instance_modify->service_list, (service*)http_tmp);
        } 
        http_tmp=NULL;
        break;
      case 'r':
        sscanf(optarg, "%i",&(proxy_instance_modify->log_max_rotate));
        break;
      case 'i':
        proxy_instance_tmp = new_proxy_instance_from_template(proxy_instance_default);
        if (!proxy_instance_tmp) {
          unexpected_exit (11,"Cannot allocate proxy_instance");
        }
        strncpy(proxy_instance_tmp->name, optarg, PROXY_INSTANCE_MAX_NAME_LEN-1);
        proxy_instance_tmp->name[PROXY_INSTANCE_MAX_NAME_LEN-1]=0; // ensure null terminated
        proxy_instance_list = insert_proxy_instance(proxy_instance_list, proxy_instance_tmp);
        proxy_instance_modify = proxy_instance_tmp;
        proxy_instance_tmp=NULL;
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
    printf("  -s name:socks5port:command\n");
    printf("          Specify an SSH command to use for tunnels. SSH tunnels are global, shared by all proxy instances.\n");
    printf("            name       = string. Uniquely identifies this SSH command. Can be used in routing rules. Printed in logs and web UI.\n");
    printf("            socks5port = local port the SSH SOCKS5 server listens on.\n");
    printf("            command = bash-interpreted command to launch SSH for this target\n");
    printf("          Because the command-portion likely contains spaces, its recommended you enclose this parameter in quotes.\n");
    printf("          Example:\n");
    printf("            -s \"vicbas:viaProd:4200:ssh -A -D 4200 vicbas03\"\n");
    printf("  -i <name>  new proxy instance \n");
    printf("          A Proxy Instance can have zero or more SOCKS, HTTP and PortForward servers. \n");
    printf("          Each Proxy Instance has its own, independent logfile config and verbosity level.\n");
    printf("          Subsequent SOCKS, HTTP and PortForward servers are associated with this Proxy Instance\n");
    printf("          until a new Proxy Instance is created with a new -i.\n");
    printf("          If you specify no Proxy Instances, a default one is allocated for you.\n");
    printf("\n");
    printf("Services: \n");
    printf("  -p [bind_address:]port\n");
    printf("          Create a SOCKS4/5 server on the local bind_address (default 127.0.0.1) and port\n");
    printf("  -L [bind_address:]port:host:hostport\n");
    printf("          Create a port-forward from the local bind_addres (default 127.0.0.1) and port\n");
    printf("          to the remote host and hostport.\n");
    printf("          Mimics 'ssh -L'. See 'man ssh' option -L for more information.\n"); 
    printf("  -m [bind_address:]port:base_directory\n");
    printf("          Create an HTTP monitor server on the local bind_address (default 127.0.0.1) and port.\n");
    printf("          base_directory should be set to the SmartSOCKSProxy web artifacts directory.\n");
    printf("\n");
    printf("Logfile:\n");
    printf("  -l <log file name>\n");
    printf("          Set to '-' to log to STDOUT.\n");
    printf("  -S <size>\n");
    printf("          Max logfile size, in bytes, before rotating logfile.\n");
    printf("  -r <num>\n");
    printf("          When rotating logfile, keep <num> previous logfiles before deleting the oldest one.\n");
    printf("  -v      increase log verbosity\n");
    printf("  -V <level>\n");
    printf("          Set log verbosity to <level> (see log.h):\n");
    printf("             error\n");
    printf("             warn\n");
    printf("             info\n");
    printf("             debug\n");
    printf("             trace\n");
    printf("             trace2\n");
    printf("\n");
    printf("Other Options:\n");
    printf("  -d      daemonize; start proxy in background\n");
    printf("  -h      print this help\n");
    printf("\n");

    return 2;
  }
  if (error) {
    return 1;
  }

  if (daemonize && call_daemon() < 0) {
    unexpected_exit(12,"daemon()");
  }

  if (set_ulimit(4096) != 0) {
    unexpected_exit(13,"set_ulimit()");
  }

  // backwards-compatible behavior
  if (proxy_instance_list == NULL) {
    proxy_instance_list = proxy_instance_default;
  }

  // initialize logfiles
  // TODO FIXME: figure out and configure logging config for main thread 
  for (proxy_instance *pinst = proxy_instance_list; pinst; pinst = pinst->next) {
    pinst->log = new_log_info( pinst->log_file_name, pinst->log_level, pinst->log_max_size, pinst->log_max_rotate ); 
    if (pinst->log == NULL) {
      unexpected_exit (14,"Errors initializing log files. Exiting.");
    }
  }
  return server(proxy_instance_list,ssh_tunnel_list);
}

