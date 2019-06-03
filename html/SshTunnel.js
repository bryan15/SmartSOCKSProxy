// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

class SshTunnel extends React.Component {
  constructor(props) {
    super(props);
  }
  render2() {
    return(<div></div>);
  }
  render() {
    let ssh=this.props.sshTunnel;
    let key="ssh_"+ssh.name;

    let status = ssh.socksPort + " " + ssh.name ;
    if (ssh.startTime) {
      status += " ACTIVE (pid " + ssh.pid+") ";
    } else {
      status += " unused ";
    }
    return (
      <div><small>SSH port {status}</small></div>
    );
  }
}


