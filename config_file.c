// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>

#include"log.h"
#include"proxy_instance.h"
#include"ssh_tunnel.h"
#include"unit_test.h"
#include"string2.h"
#include"service.h"
#include"service_socks.h"
#include"service_port_forward.h"
#include"service_http.h"

#define MAX_LINE_LENGTH 10240

// returns: 
//   0 = error
//   1 = null-terminated line returned in 'buf', still more to read from file.
//   2 = null-terminated line returned in 'buf'. End of file reached.
int read_line(int fd, char* buf, int buflen, int* cr, int* lf, char *filename, int line_num) {
  int idx=0;
  int loop=1;
  char tmp[10];
  buf[idx]=0;

  // This is inefficient reading one byte at a time. But... it only happens once at boot. 
  while(1) {
    int rc;
    char chr;
    do {
      rc = read(fd, &chr, 1);
    } while (rc < 0 && (errno == EINTR || errno == EAGAIN));
    if (rc < 0) {
      errorNum("Error reading file '%s'",filename);
      return 0;
    }
    if (rc == 0) { // end of file
      return 2;
    }
    
    // little piece 'o logic to detect line endings despite arbitrary combinations of \r and \n
    if (chr == '\n' && *cr == 1) {
      // eat this character - it belongs to the previous line break. Reset markers.
      *cr=*lf=0;
    } else if (chr == '\r' && *lf == 1) {
      // eat this character - it belongs to the previous line break. Reset markers.
      *cr=*lf=0;
    } else if (chr == '\n') {
      *lf=1;
      return 1;
    } else if (chr == '\r') {
      *cr=1; 
      return 1;
    } else {
      *cr=*lf=0;
      buf[idx]=chr;
      idx++;
      buf[idx]=0;
      if (idx >= buflen-1) {
        error("File %s Line %i too long, cannot parse!!", filename, line_num);
        return 0;
      }   
    }
  }
}

#define CHECK_FOR_SPACE do { \
      if (need_a_space) { \
        *outptr=' ';\
        outptr++;\
        need_a_space=0;\
      } \
    } while(0)

int remove_extra_spaces_and_comments_from_config_line(char *inbuf, char *outbuf, int outbuflen) {
  char* inptr = inbuf; 
  char* outptr = outbuf;
  int escape=0;
  int comment=0;
  int need_a_space=0;
  int quote_single=0;
  int quote_double=0;
 
  for (inptr=inbuf;*inptr;inptr++) {
    if (*inptr == '\\' && escape == 0) {
      escape = 1;
    } else if (escape == 1) {
      CHECK_FOR_SPACE;
      *outptr = *inptr; 
      outptr++;
      escape=0;
    } else if ((quote_single && *inptr == '\'')||(quote_double && *inptr == '"')) {
      quote_single=quote_double=0;
      *outptr = *inptr; 
      outptr++;
    } else if (quote_single || quote_double) {
      *outptr = *inptr; 
      outptr++;
    } else if (*inptr == '\'') {
      CHECK_FOR_SPACE;
      *outptr = *inptr; 
      outptr++;
      quote_single=1;
    } else if (*inptr == '"') {
      CHECK_FOR_SPACE;
      *outptr = *inptr; 
      outptr++;
      quote_double=1;
    } else if (*inptr == '#') {
      // we're done. comments run to the end of the line. 
      comment=1;
      break;
    } else if (outptr == outbuf && (*inptr == ' ' || *inptr == '\t')) {
      // eat leading space
    } else if (*inptr == ' ' || *inptr == '\t') {
      need_a_space=1;
    } else {
      CHECK_FOR_SPACE;
      *outptr = *inptr; 
      outptr++;
    }
  }
  *outptr=0;
  if (quote_single) {
    error("Line has unmatched single-quote (\'): %s", inbuf);
    return 0;
  }
  if (quote_double) {
    error("Line has unmatched double-quote (\"): %s", inbuf);
    return 0;
  }
  return 1;
}

// if line starts-with match, return first position of parameter, else NULL
char* match_start(char *line, char *match) {
  if (string_starts_with(line,match)) {
    return &(line[strlen(match)]);
  } 
  return NULL;
}

////////////////////////////////// ////////////////////////////////// ////////////////////////////////// //////////////////////////////////
////////////////////////////////// ////////////////////////////////// ////////////////////////////////// //////////////////////////////////

int config_file_parse_main(char *filename, int line_num, char *line) {
  if (strcmp(line,"main")==0) {
    return 1;
  }
  return 0;
}

log_file* config_file_parse_log_file(char *filename, int line_num, char *line, log_file** log_file_list, log_file* log_file_default) {
  if (!string_starts_with(line,"logfile ")) { // TODO: convert to use match_start()
    return NULL;
  }
  char *section = &(line[8]);
  if (strlen(section)<=0) {
    error("(%s line %i) Invalid logfile definition: %s",filename, line_num, line);
    exit(1);
  }
  if (strcmp(section,"default")==0) { 
    debug("(%s line %i) switch log_file to default",filename, line_num);
    return log_file_default;
  }
  log_file *log = find_or_create_log_file(log_file_list, log_file_default, section);
  debug("(%s line %i) switch log_file to '%s'",filename, line_num, log->file_name);
  return log;
}

proxy_instance* config_file_parse_proxy_instance(char *filename, int line_num, char *line, proxy_instance** proxy_instance_list, proxy_instance *proxy_default) {
  if (!string_starts_with(line,"proxy ")) { // TODO: convert to use match_start()
    return NULL;
  }

  char *section = &(line[6]);
  if (strlen(section)<=0) {
    error("(%s line %i) Invalid proxy definition: %s",filename, line_num, line);
    exit(1);
  }
  if (strcmp(section,"default")==0) { 
    debug("(%s line %i) switch proxy_instance to default",filename, line_num);
    return proxy_default;
  }
  for (proxy_instance* proxy = *proxy_instance_list; proxy != NULL ; proxy=proxy->next) {
    if (strcmp(section,proxy->name)==0) {
      debug("(%s line %i) switch proxy_instance to '%s'",filename, line_num, proxy->name);
      return proxy;
    }
  }

  // we didn't find this proxy in our list. Create it and return the new value. 
  proxy_instance* new_proxy = new_proxy_instance_from_template(proxy_default);
  strncpy(new_proxy->name,section,sizeof(new_proxy->name)-1);
  *proxy_instance_list = insert_proxy_instance(*proxy_instance_list, new_proxy);
  debug("(%s line %i) create new proxy_instance '%s'",filename, line_num, new_proxy->name);
  return new_proxy;
}

ssh_tunnel* config_file_parse_ssh_tunnel(char *filename, int line_num, char *line, ssh_tunnel** ssh_tunnel_list, ssh_tunnel* ssh_default) {
  if (!string_starts_with(line,"ssh ")) {
    return NULL;
  }
  char *section = &(line[4]);
  if (strlen(section)<=0) {
    error("(%s line %i) Invalid ssh definition: %s",filename, line_num, line);
    exit(1);
  }
  if (strcmp(section,"default")==0) { 
    debug("(%s line %i) switch ssh_tunnel to default",filename, line_num);
    return ssh_default;
  }
  for (ssh_tunnel* ssh = *ssh_tunnel_list; ssh != NULL ; ssh=ssh->next) {
    if (strcmp(section,ssh->name)==0) {
      debug("(%s line %i) switch ssh_tunnel to '%s'",filename, line_num, ssh->name);
      return ssh;
    }
  }

  // we didn't find this ssh in our list. Create it and set the current ssh to it
  ssh_tunnel *new_ssh = new_ssh_tunnel(ssh_default);
  strncpy(new_ssh->name,section,sizeof(new_ssh->name)-1);
  *ssh_tunnel_list = insert_ssh_tunnel(*ssh_tunnel_list, new_ssh);
  debug("(%s line %i) create new ssh_tunnel '%s'",filename, line_num, new_ssh->name);
  return new_ssh;
}


////////////////////////////////// ////////////////////////////////// ////////////////////////////////// //////////////////////////////////
////////////////////////////////// ////////////////////////////////// ////////////////////////////////// //////////////////////////////////

int config_set_long(char *filename, int line_num, char *line, char *object_type, char *object_name, char *field, char *help, long *dst) {
  char *valueStr;
  if ((valueStr = match_start(line, field))) {
    if (sscanf(valueStr,"%li", dst) != 1) {
      error("USAGE: %s",help);
      return 0;
    }
    debug("(%s line %i) set %s %s %s= %li", filename, line_num, object_type, object_name, field, *dst);
    return 1;
  }
  return 0;
}

int config_set_int(char *filename, int line_num, char *line, char *object_type, char *object_name, char *field, char *help, int *dst) {
  char *valueStr;
  if ((valueStr = match_start(line, field))) {
    if (sscanf(valueStr,"%i", dst) != 1) {
      error("USAGE: %s",help);
      return 0;
    }
    debug("(%s line %i) set %s %s %s= %i", filename, line_num, object_type, object_name, field, *dst);
    return 1;
  }
  return 0;
}

int config_set_string(char *filename, int line_num, char *line, char *object_type, char *object_name, char *field, char *help, char *dst, int dstLen) {
  char *valueStr;
  if ((valueStr = match_start(line, field))) {
    if (strlen(valueStr)==0) {
      error("USAGE: %s",help);
      return 0;
    }
    strncpy(dst,valueStr,dstLen-1);
    debug("(%s line %i) set %s %s %s= %s", filename, line_num, object_type, object_name, field, dst);
    return 1;
  }
  return 0;
}

////////////////////////////////// ////////////////////////////////// ////////////////////////////////// //////////////////////////////////

int config_file_parse_main_entry(char *filename, int line_num, char *line, log_config *log_config_main, log_file **log_file_list, log_file *log_file_default ) {
  char stringBuf[8192];
  if (config_set_string(filename, line_num, line, "main", "", "logFilename ","logFilename <file_name>", stringBuf, sizeof(stringBuf))) {
    log_config_main->file = find_or_create_log_file(log_file_list, log_file_default, stringBuf);
    return 1;
  }
  char* help="logVerbosity [error|warn|info|debug|trace|trace2]";
  if (config_set_string(filename, line_num, line, "main", "", "logVerbosity ",help, stringBuf, sizeof(stringBuf))) {
    log_config_main->level = log_level_from_str(stringBuf);
    if (log_config_main->level == LOG_LEVEL_INVALID) {
      error("USAGE: %s",help);
      return 0;
    }
    return 1;
  }
  return 0;
}

int config_file_parse_log_file_entry(char *filename, int line_num, char *line, log_file *log) {
  if (config_set_long(filename, line_num, line, "logfile", log->file_name, "byteCountMax ","byteCountMax <long>", &(log->byte_count_max))) return 1;
  if (config_set_long(filename, line_num, line, "logfile", log->file_name, "fileRotateCount ","fileRotateCount <long>", &(log->file_rotate_count))) return 1;
  return 0;
}

int config_file_parse_proxy_instance_entry(char *filename, int line_num, char *line, proxy_instance *proxy, log_file **log_file_list, log_file *log_file_default) {
  char stringBuf[8192];
  if (config_set_string(filename, line_num, line, "proxy", proxy->name, "logFilename ","logFilename <file_name>", stringBuf, sizeof(stringBuf))) {
    proxy->log.file = find_or_create_log_file(log_file_list, log_file_default, stringBuf);
    return 1;
  }
  char* help="logVerbosity [error|warn|info|debug|trace|trace2]";
  if (config_set_string(filename, line_num, line, "proxy", proxy->name, "logVerbosity ",help, stringBuf, sizeof(stringBuf))) {
    proxy->log.level = log_level_from_str(stringBuf);
    if (proxy->log.level == LOG_LEVEL_INVALID) {
      error("USAGE: %s",help);
      return 0;
    }
    return 1;
  }
  
  // socks 4/5 server
  help = "socksServer ";
  if (config_set_string(filename, line_num, line, "proxy", proxy->name, "socksServer ",help, stringBuf, sizeof(stringBuf))) {
    service_socks *socks_tmp = parse_service_socks_spec(stringBuf);
    if (socks_tmp == NULL) {
      error("USAGE: %s",help);
      return 0;
    }
    proxy->service_list = insert_service(proxy->service_list, (service*)socks_tmp);
    return 1;
  }

  // SSH port-forward
  help = "portForward ";
  if (config_set_string(filename, line_num, line, "proxy", proxy->name, "portForward ",help, stringBuf, sizeof(stringBuf))) {
    service_port_forward *port_forward_tmp = parse_service_port_forward_spec(stringBuf);
    if (port_forward_tmp == NULL) {
      error("USAGE: %s",help);
      return 0;
    }
    proxy->service_list = insert_service(proxy->service_list, (service*)port_forward_tmp);
    return 1;
  }

  // HTTP Server for viewing SmartSOCKSProxy status
  help = "httpServer ";
  if (config_set_string(filename, line_num, line, "proxy", proxy->name, "httpServer ",help, stringBuf, sizeof(stringBuf))) {
    service_http *http_tmp = parse_service_http_spec(stringBuf);
    if (http_tmp == NULL) {
      error("USAGE: %s",help);
      return 0;
    }
    proxy->service_list = insert_service(proxy->service_list, (service*)http_tmp);
    return 1;
  }

  
  // TODO: routing rules

  return 0;
}

int config_file_parse_ssh_tunnel_entry(char *filename, int line_num, char *line, ssh_tunnel *ssh, log_file **log_file_list, log_file *log_file_default) {
  char stringBuf[8192];
  if (config_set_int(filename, line_num, line, "ssh", ssh->name, "socksPort ","socksPort <int>", &(ssh->socks_port))) return 1;
  if (config_set_string(filename, line_num, line, "ssh", ssh->name, "command ","command <shell_command_to_start_ssh>", ssh->command_to_run, sizeof(ssh->command_to_run)))  return 1;

  if (config_set_string(filename, line_num, line, "ssh", ssh->name, "logFilename ","logFilename <file_name>", stringBuf, sizeof(stringBuf))) {
    ssh->log.file = find_or_create_log_file(log_file_list, log_file_default, stringBuf);
    return 1;
  }
  char* help="logVerbosity [error|warn|info|debug|trace|trace2]";
  if (config_set_string(filename, line_num, line, "ssh", ssh->name, "logVerbosity ", help, stringBuf, sizeof(stringBuf))) {
    ssh->log.level = log_level_from_str(stringBuf);
    if (ssh->log.level == LOG_LEVEL_INVALID) {
      error("USAGE: %s",help);
      return 0;
    }
    return 1;
  }
  return 0;
}

////////////////////////////////// ////////////////////////////////// ////////////////////////////////// //////////////////////////////////
////////////////////////////////// ////////////////////////////////// ////////////////////////////////// //////////////////////////////////

int config_file_parse(log_file **log_file_list, log_file *log_file_default, 
                      log_config *log_config_main, 
                      proxy_instance** proxy_instance_list, proxy_instance* proxy_default, 
                      ssh_tunnel** ssh_tunnel_list, ssh_tunnel *ssh_default, 
                      char* filename, char **filename_stack, int filename_stack_size, int filename_stack_index) {

  debug("Reading config file '%s'",filename);
  filename_stack[filename_stack_index]=filename;   
  filename_stack_index++;

  int fd = open(filename,O_RDONLY);
  if ( fd < 0 ) {
    error("Cannot open config file '%s'",filename);
    return 0;
  }

  int line_num = 0;
  int cr = 0;
  int lf = 0;
  char buf[MAX_LINE_LENGTH];
  char buf2[MAX_LINE_LENGTH];
  int rc;

  int main=0;
  log_file* log_current = NULL;
  proxy_instance* proxy_current = NULL;
  ssh_tunnel* ssh_current = NULL;

  int okay=1;
  while(okay) {
    line_num++;

    okay = read_line(fd, buf, sizeof(buf), &cr, &lf, filename, line_num);  
    if (okay == 2) { // eof
      break;
    }
    if (!okay) {
      return 0; 
    }

    // parse this line
    trace2("config (%s line %i, %i bytes): %s", filename, line_num, strlen(buf), buf);
    if (!remove_extra_spaces_and_comments_from_config_line(buf,buf2,sizeof(buf2))) {
      return 0;
    }

    char *line = buf2;

    if (strlen(line)==0) {
      continue;
    }

    char include_filename[8192];
    if (config_set_string(filename, line_num, line, "main", "", "include ","include <file_name>", include_filename, sizeof(include_filename))) {
      // we've been asked to include a file. First, let's check if we've already included the file - no infinite loops for us! I hope. 
      int found_file_in_stack = 0;
      for (int i=0; !found_file_in_stack && i < filename_stack_index; i++) {
        if (strcmp(filename_stack[i],include_filename)==0) {
          found_file_in_stack=1;
        }
      }
      if (found_file_in_stack) {
        warn("Attempt to include file \"%s\" blocked; we've already included it, and recursive config files is disallowed.",include_filename);
      } else if (filename_stack_index >= filename_stack_size) {
        warn("Attempt to include file \"%s\" blocked; include file recursion cannot go deeper than %i", include_filename, filename_stack_size);
      } else {
        config_file_parse(log_file_list, log_file_default, log_config_main, 
                      proxy_instance_list, proxy_default, 
                      ssh_tunnel_list, ssh_default, 
                      include_filename, filename_stack, filename_stack_size, filename_stack_index);
      }
      continue;
    }

    int main_tmp = config_file_parse_main(filename, line_num, line);
    if (main_tmp) {
      main = main_tmp;
      log_current = NULL;
      proxy_current = NULL;
      ssh_current = NULL;
      continue;
    }
    log_file *log_tmp = config_file_parse_log_file(filename, line_num, line, log_file_list, log_file_default);
    if (log_tmp) {
      main = 0;
      log_current = log_tmp;
      proxy_current = NULL;
      ssh_current = NULL;
      continue;
    }
    proxy_instance *proxy_tmp = config_file_parse_proxy_instance(filename, line_num, line, proxy_instance_list, proxy_default);
    if (proxy_tmp) {
      main = 0;
      log_current = NULL;
      proxy_current = proxy_tmp;
      ssh_current = NULL;
      continue;
    }
    ssh_tunnel *ssh_tmp = config_file_parse_ssh_tunnel(filename, line_num, line, ssh_tunnel_list, ssh_default);
    if (ssh_tmp) {
      main = 0;
      log_current = NULL;
      proxy_current = NULL;
      ssh_current = ssh_tmp;
      continue;
    }
    
    if (main && config_file_parse_main_entry(filename, line_num, line, log_config_main, log_file_list, log_file_default)) {
      continue;
    }
    if (log_current && config_file_parse_log_file_entry(filename, line_num, line, log_current)) {
      continue;
    }
    if (proxy_current && config_file_parse_proxy_instance_entry(filename, line_num, line, proxy_current, log_file_list, log_file_default)) {
      continue;
    }
    if (ssh_current && config_file_parse_ssh_tunnel_entry(filename, line_num, line, ssh_current, log_file_list, log_file_default)) {
      continue;
    }
 
    error("Invalid Config in %s line %i:  \"%s\"",filename, line_num, line);
    exit(1);
  }

  debug("Done reading %s",filename);
  return 1;
}

