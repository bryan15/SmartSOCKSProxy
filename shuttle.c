// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include<sys/types.h>
#include<sys/uio.h>
#include<unistd.h>
#include<sys/select.h>
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
  fd_set readfds, writefds, errorfds;
  struct timeval timeout;
  int maxfd;
  int rc;

  trace("shuttle started");
  trace("FD = %i %i",con->fd_in, con->fd_out);

  // General Comment: This loop structure is rather stupid
  // in that it blocks on read and write operations. IE:
  // a blocking operation for data in one direction will
  // also block data moving in the opposite direction. 
  // But its easy to write this way, so, whatever...
  int continue_loop = 1;
  while (continue_loop) {
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&errorfds);

    FD_SET(con->fd_in,&readfds);
    FD_SET(con->fd_in,&errorfds);
    FD_SET(con->fd_out,&readfds);
    FD_SET(con->fd_out,&errorfds);

    if (con->fd_out > con->fd_in) {
      maxfd=con->fd_out;
    } else {
      maxfd=con->fd_in;
    }

    //timeout.tv_sec=1;
    //timeout.tv_usec=0;

    trace2("select()... %i %i ",con->fd_in, con->fd_out);
    do {
      rc=select(maxfd+1, &readfds,&writefds,&errorfds,NULL);
    } while (rc < 0 && (errno == EAGAIN || errno == EINTR));
    trace2("... select() returned %i",rc);

    if (FD_ISSET(con->fd_in,&errorfds)) {
      errorNum("Error on fd_in socket. Exiting.");
      return 0;
    }
    if (FD_ISSET(con->fd_out,&errorfds)) {
      errorNum("Error on fd_out socket. Exiting.");
      return 0;
    }

    if (FD_ISSET(con->fd_in,&readfds)) {
      rc=shuttle(con, con->fd_in, con->fd_out);
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
        trace2("shuttle in->out %i bytes",rc);
      }
    }
    if (FD_ISSET(con->fd_out,&readfds)) {
      rc=shuttle(con, con->fd_out, con->fd_in);
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
      //  trace("shuttle in->out %i bytes",rc);
      }
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
  

