// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

class Connection extends React.Component {
  constructor(props) {
    super(props);
  }

  render() {
    let con=this.props.connection;
    let service=this.props.service;

    let status = "";
    if (con.timeEnd) {
      status += "Closed ";
    }
    if (con.status != 0) {
      status += "("+con.status+") ";
    }
    if (con.statusName) {
      status += con.statusName+" ";
    }
    if (con.statusDescription) {
      if (con.statusName) {
        status += " : ";
      }
      status += con.statusDescription+" ";
    }
    if (service.type == "SOCKS") {
      return (
        <div>Connection {con.connectionId} {service.type}{con.socksVersion} &nbsp;
          {con.sourceAddress}:{con.sourcePort} &rarr; &nbsp;
          {con.route} &rarr; &nbsp;
          {con.remoteAddress}:{con.remotePort} ({con.remoteAddressType}) &nbsp;
          ({con.remoteAddressEffective}) &nbsp;
          Rx {con.bytesRx} Tx {con.bytesTx} &nbsp;
          {status}
        </div>
      );
    } else if (service.type == "portForward") {
      return (
        <div>Connection {con.connectionId} {service.type} &nbsp;
          {service.localAddress}:{service.localPort} &rarr; &nbsp;
          {con.route} &rarr; &nbsp;
          {con.remoteAddress}:{con.remotePort} ({con.remoteAddressType}) &nbsp;
          ({con.remoteAddressEffective}) &nbsp;
          Rx {con.bytesRx} Tx {con.bytesTx} &nbsp;
          {status}
        </div>
      );
    } else if (service.type == "HTTP") {
      return (
        <div>Connection {con.connectionId} {service.type} {con.urlPath} &nbsp;
          {status}
        </div>
      );
    } else {
      return (
        <div>Connection {con.connectionId} {service.type} &nbsp;
          {status}
        </div>
      );
    }
  }
}


