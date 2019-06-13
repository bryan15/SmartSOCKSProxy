// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<sys/types.h>
#include<sys/uio.h>
#include<unistd.h>
#include<poll.h>
#include<errno.h>
#include<string.h>

#include"log.h"
#include"client_connection.h"
#include"safe_blocking_readwrite.h"

// returns 
//    <0  error
//    =0  end of transmission on fd_read
//    >0  bytes read from fd_read and written to fd_write
int shuttle(client_connection *con, int fd_read, int fd_write) {
  unsigned char buf[10000];
  int read_rc=sb_read(fd_read,buf,sizeof(buf));
  if (read_rc == 0) { // end of transmission
    trace("connection closed normally.");
  } else if (read_rc < 0 && errno == ECONNRESET ) {
    set_client_connection_status(con,errno,"Error",strerror(errno));
    trace("connection reset.");
  } else if (read_rc < 0) {
    set_client_connection_status(con,errno,"Error",strerror(errno));
    errorNum("read()");
  } else if (fd_write <0) { // -1 if we cannot write
    // merely drain the data
  } else {
    buf[read_rc]=0;
    //trace(" (%i) >> %s",read_rc,buf);
    int write_rc = sb_write_len(fd_write,buf,read_rc);
    if (write_rc<0) {
      errorNum("write()");
    //} else {
    //  trace("%i bytes written",write_rc);
    }
    return write_rc;
  }
  return read_rc;
}

// IMPROVEMENT: not all errors are reported to the WebUI via set_client_connection_status(). This could be improved. 
// return 1 if exit cleanly, 0 on error
int shuttle_data_back_and_forth(client_connection *con) {
  struct pollfd pfd[3];
  int pfd_max;
  int pfd_idx;
  int timeout;
  int rc;

  int in_can_write = 1;
  int out_can_write = 1;

  trace("shuttle started");
  trace("FD = %i %i",con->fd_in, con->fd_out);

  // General Comment: This loop structure is rather stupid
  // in that it blocks on read and write operations. IE:
  // a blocking operation for data in one direction will
  // also block data moving in the opposite direction. 
  // But its easy to write this way, so, whatever...
  int continue_loop = 1;
  while (continue_loop) {

    pfd[0].fd = con->fd_in;
    pfd[0].events = POLLRDNORM;
    pfd[0].revents = 0;

    pfd[1].fd = con->fd_out;
    pfd[1].events = POLLRDNORM;
    pfd[1].revents = 0;
   
    pfd_max=2;
    timeout = -1; // block indefinitely

    trace2("poll()... %i %i ",con->fd_in, con->fd_out);
    do {
      rc = poll(pfd,pfd_max,timeout);
    } while (rc < 0 && errno == EINTR);
    trace2("... poll() returned %i",rc);
    if (rc < 0) {
      errorNum("poll()");
      return 0;
    }

    for (int i=0;i<pfd_max;i++) {
      short tmp = pfd[i].revents;
      tmp |= POLLRDNORM;
      tmp ^= POLLRDNORM;
      if (tmp != 0) {
        trace("poll() %i  fd %i revents = %s %s %s %s %s %s %s %s", i, pfd[i].fd,
          pfd[i].revents & POLLERR ? "POLLERR" : "",
          pfd[i].revents & POLLHUP ? "POLLHUP" : "",
          pfd[i].revents & POLLNVAL ? "POLLNVAL" : "",
          pfd[i].revents & POLLPRI ? "POLLPRI" : "",
          pfd[i].revents & POLLRDBAND ? "POLLRDBAND" : "",
          pfd[i].revents & POLLRDNORM ? "POLLRDNORM" : "",
          pfd[i].revents & POLLWRBAND ? "POLLWRBAND" : "",
          pfd[i].revents & POLLWRNORM ? "POLLWRNORM" : ""
          );
      }
    }

    if (pfd[0].revents & POLLHUP) {
      in_can_write=0;
    }
    if (pfd[1].revents & POLLHUP) {
      out_can_write=0;
    }

    if (pfd[0].revents & (POLLERR | POLLNVAL)) {
      errorNum("Error on fd_in socket. Exiting.");
      return 0;
    }
    if (pfd[1].revents & (POLLERR | POLLNVAL)) {
      errorNum("Error on fd_out socket. Exiting.");
      return 0;
    }

    if (pfd[0].revents & POLLRDNORM) {
      if (out_can_write) {
        rc=shuttle(con, con->fd_in, con->fd_out);
      } else {
        rc=shuttle(con, con->fd_in, -1);
      }
      if (rc<0) {
        return 0; // error
      } else if (rc == 0) {
        continue_loop=0;
        close(con->fd_in);
        con->fd_in=-1; 
      } else {
        lock_client_connection(con);
        con->bytes_tx += rc;
        unlock_client_connection(con);
        //trace2("shuttle in->out %i bytes",rc);
      }
    }
    if (pfd[1].revents & POLLRDNORM) {
      if (in_can_write) {
        rc=shuttle(con, con->fd_out, con->fd_in);
      } else {
        rc=shuttle(con, con->fd_out, -1);
      }
      if (rc<0) {
        return 0; // error
      } else if (rc == 0) {
        continue_loop=0;
        close(con->fd_out);
        con->fd_out=-1;
      } else {
        lock_client_connection(con);
        con->bytes_rx += rc;
        unlock_client_connection(con);
      //  trace("shuttle out->in %i bytes",rc);
      }
    }

    if (!(in_can_write || (pfd[0].revents&POLLRDNORM))) {
      continue_loop=0;
    }
    if (!(out_can_write || (pfd[1].revents&POLLRDNORM))) {
      continue_loop=0;
    }

  } 
  // don't need to use mutex because only the connection thread changes these values -- and we're the connection thread :)
  trace("shuttle connection closed normally. Tx %llu  Rx %llu  bytes",con->bytes_tx,con->bytes_rx);
  return 1;
}

int shuttle_null_connection(client_connection *con) {
  int rc;
  unsigned char buf[10];
  while(1) {
    rc=sb_read_len(con->fd_in, buf, 1);
    if (rc == 1) {
      lock_client_connection(con);
      con->bytes_rx += 1;
      unlock_client_connection(con);
    } else {
      set_client_connection_status(con,errno,"Error",strerror(errno));
      break;
    }
  }
  if (rc==0) {
    return 1;
  }
  return 0;
}
  

