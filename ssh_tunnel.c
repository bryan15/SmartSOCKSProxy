#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>

#include"log.h"
#include"ssh_tunnel.h"

unsigned long long ssh_id_pool=0;

ssh_tunnel *ssh_tunnel_direct;
ssh_tunnel *ssh_tunnel_null;

ssh_tunnel *new_ssh_tunnel() {
  ssh_id_pool++;
  trace("new_ssh_tunnel(%llu)",ssh_id_pool);
  ssh_tunnel *ssh = malloc(sizeof(ssh_tunnel));
  if (ssh == NULL) {
    errorNum("Error allocating new ssh tunnel");
    unexpected_exit(21,"Error allocating new ssh tunnel");
  }
  ssh->id=ssh_id_pool;
  ssh->next=NULL;
  ssh->command_to_run[0]=0;
  ssh->socks_port=0;

  ssh->pid=-1;
  ssh->start_time=0;
  // We don't want child processes writing to STDOUT and STDERR. 
  // Instead, we'll rediret their output to a pipe. Of course, 
  // someone needs to read the pipe and write the contents
  // to the logfile; this is done in the main listening loop. 
  ssh->parent_stdin_fd=-1;
  ssh->parent_stdout_fd=-1;
  ssh->parent_stderr_fd=-1;
  ssh->child_stdin_fd=-1;
  ssh->child_stdout_fd=-1;
  ssh->child_stderr_fd=-1;

  return ssh;
}

ssh_tunnel *insert_ssh_tunnel(ssh_tunnel *head, ssh_tunnel *ssh) {
  trace("insert_ssh_tunnel()");
  // ssh is the new head
  ssh->next=head;
  return ssh;
}

ssh_tunnel *ssh_tunnel_init(ssh_tunnel *head) {
  ssh_tunnel_direct = new_ssh_tunnel();
  strncpy(ssh_tunnel_direct->name,"direct",sizeof(ssh_tunnel_direct->name)-1);
  head=insert_ssh_tunnel(head,ssh_tunnel_direct);
 
  ssh_tunnel_null = new_ssh_tunnel();
  strncpy(ssh_tunnel_null->name,"null",sizeof(ssh_tunnel_null->name)-1);
  head=insert_ssh_tunnel(head,ssh_tunnel_null);

  return head;
}

int close_fd(int fd) {
  if (fd > -1) { 
    close(fd);
  }
  return -1;
}

// assumes ssh->pid has exited 
void reset_ssh_tunnel(ssh_tunnel *ssh) {
  ssh->parent_stdin_fd = close_fd(ssh->parent_stdin_fd);
  ssh->parent_stdout_fd = close_fd(ssh->parent_stdout_fd);
  ssh->parent_stderr_fd = close_fd(ssh->parent_stderr_fd);
  ssh->child_stdin_fd = close_fd(ssh->child_stdin_fd);
  ssh->child_stdout_fd = close_fd(ssh->child_stdout_fd);
  ssh->child_stderr_fd = close_fd(ssh->child_stderr_fd);
  ssh->pid=-1;
  ssh->start_time = 0;
}

char *ssh_tunnel_str(ssh_tunnel *ssh, char *buf, int buflen) {
  snprintf(buf,buflen-1,"%llu:%s:%i:\"%s\"",ssh->id,ssh->name,ssh->socks_port,ssh->command_to_run);
  return buf;
}

ssh_tunnel *parse_ssh_tunnel_spec(char *str) {
  char *token;
  char *strPtr;
  char *local_copy;
  int okay = 1;
  ssh_tunnel* ssh=NULL;

  local_copy = strPtr = strdup(str);
  if (strPtr == NULL) {
    okay = 0;
  } 
  if (okay) {
    ssh = new_ssh_tunnel();
    if (ssh == NULL) {
      okay=0;
    } 
  }

  char *name;
  char *socks5portStr;
  char *command;
  if (okay) {
    name=strsep(&strPtr,":");
    socks5portStr=strsep(&strPtr,":");
    command=strPtr;
  }
  if (command == NULL) {
    okay=0;
  } else {
    strncpy(ssh->name,name,sizeof(ssh->name));
    sscanf(socks5portStr,"%i",&(ssh->socks_port));
    strncpy(ssh->command_to_run,command,sizeof(ssh->command_to_run));
  }
  
  // clean up after ourselves
  if (local_copy != NULL) {
    free(local_copy);
    local_copy=NULL;
  }

  if (!okay) {
    if (ssh != NULL) {
      error("error parsing ssh_tunnel");
      free(ssh);
      ssh=NULL;
    }
  } else {
    char tmp[12000];
    trace("Parsed ssh tunnel: %s",ssh_tunnel_str(ssh,tmp,sizeof(tmp)));
  }

  return ssh;

}

