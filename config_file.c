// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>

#include"log.h"
#include"proxy_instance.h"
#include"ssh_tunnel.h"
#include"unit_test.h"
#include"string2.h"

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

proxy_instance* config_file_parse_proxy_instance(char *filename, int line_num, char *line, proxy_instance** proxy_instance_list, proxy_instance *proxy_default) {
  if (!string_starts_with(line,"proxy ")) {
    return NULL;
  }

  char *section = &(line[6]);
  if (strlen(section)<=0) {
    error("%s line %i: Invalid proxy definition: %s",filename, line_num, line);
    return NULL;
  }
  if (strcmp(section,"default")==0) { 
    trace("%s line %i: switch proxy_instance to default",filename, line_num);
    return proxy_default;
  }
  for (proxy_instance* proxy = *proxy_instance_list; proxy != NULL ; proxy=proxy->next) {
    if (strcmp(section,proxy->name)==0) {
      trace("%s line %i: switch proxy_instance to '%s'",filename, line_num, proxy->name);
      return proxy;
    }
  }

  // we didn't find this proxy in our list. Create it and return the new value. 
  proxy_instance* new_proxy = new_proxy_instance_from_template(proxy_default);
  strncpy(new_proxy->name,section,sizeof(new_proxy->name)-1);
  *proxy_instance_list = insert_proxy_instance(*proxy_instance_list, new_proxy);
  trace("%s line %i: create new proxy_instance '%s'",filename, line_num, new_proxy->name);
  return new_proxy;
}

ssh_tunnel* config_file_parse_ssh_tunnel(char *filename, int line_num, char *line, ssh_tunnel** ssh_tunnel_list, ssh_tunnel* ssh_default) {
  if (!string_starts_with(line,"ssh ")) {
    return NULL;
  }
  char *section = &(line[4]);
  if (strlen(section)<=0) {
    error("%s line %i: Invalid ssh definition: %s",filename, line_num, line);
    return NULL;
  }
  if (strcmp(section,"default")==0) { 
    trace("%s line %i: switch ssh_tunnel to default",filename, line_num);
    return ssh_default;
  }
  for (ssh_tunnel* ssh = *ssh_tunnel_list; ssh != NULL ; ssh=ssh->next) {
    if (strcmp(section,ssh->name)==0) {
      trace("%s line %i: switch ssh_tunnel to '%s'",filename, line_num, ssh->name);
      return ssh;
    }
  }

  // we didn't find this ssh in our list. Create it and set the current ssh to it
  ssh_tunnel *new_ssh = new_ssh_tunnel(ssh_default);
  strncpy(new_ssh->name,section,sizeof(new_ssh->name)-1);
  *ssh_tunnel_list = insert_ssh_tunnel(*ssh_tunnel_list, new_ssh);
  trace("%s line %i: create new ssh_tunnel '%s'",filename, line_num, new_ssh->name);
  return new_ssh;
}



int config_file_parse_one_line(proxy_instance** proxy_instance_list, proxy_instance* proxy_default, proxy_instance** proxy_current, 
                               ssh_tunnel** ssh_tunnel_list, ssh_tunnel* ssh_default, ssh_tunnel** ssh_current, 
                               char *filename, int line_num, char *line) {
 
  return 1;
}


int config_file_parse(proxy_instance** proxy_instance_list, proxy_instance* proxy_default, ssh_tunnel** ssh_tunnel_list, ssh_tunnel *ssh_default, char* filename) {
  trace("Reading config file '%s'",filename);
  int fd = open(filename,O_RDONLY);
  if ( fd < 0 ) {
    error("Cannot open config file '%s'",filename);
    return 0;
  }

  int line_num = 1;
  int cr = 0;
  int lf = 0;
  char buf[MAX_LINE_LENGTH];
  char buf2[MAX_LINE_LENGTH];
  int rc;

  proxy_instance* proxy_current = proxy_default;
  ssh_tunnel* ssh_current = ssh_default; 

  do {
    rc = read_line(fd, buf, sizeof(buf), &cr, &lf, filename, line_num);  
    if ( rc == 0) {
      return 0; 
    }
    // parse this line
    trace2("config (%s line %i, %i bytes): %s", filename, line_num, strlen(buf), buf);
    if (!remove_extra_spaces_and_comments_from_config_line(buf,buf2,sizeof(buf2))) {
      return 0;
    }
    trace2("Stripped and cleaned: '%s'",buf2);
    if (!config_file_parse_one_line(proxy_instance_list, proxy_default, &proxy_current, ssh_tunnel_list, ssh_default, &ssh_current, filename, line_num, buf2)) {
      return 0;
    }
    line_num++;
  } while (rc == 1); 

  trace2("Done reading %s",filename);
  return 1;
}

