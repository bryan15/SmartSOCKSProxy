// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

class SmartSOCKSProxy extends React.Component {
  constructor(props) {
    super(props);

    this.state={};
    this.state.selectedTab=props.selectedTab;
    this.state.connectionState={};
    this.state.updateActive=false;
    this.state.updateInterval=1000; // IMPROVEMENT: add a way to change this at run-time

    this.proxyStartTime=0;

    this.activateUpdate = this.activateUpdate.bind(this);
    this.deactivateUpdate = this.deactivateUpdate.bind(this);
    this.initiateConnectionStateUpdate = this.initiateConnectionStateUpdate.bind(this);
    this.handleConnectionStateUpdate = this.handleConnectionStateUpdate.bind(this);

    this.request = new XMLHttpRequest(); 
    this.request.onreadystatechange = this.handleConnectionStateUpdate;
    this.requestIsInFlight=false; 
    this.requestIntervalId=null;
  }

  componentDidMount() {
    this.activateUpdate();
  }
  componentWillUnmount() {
    this.deactivateUpdate();
  }

  activateUpdate() {
    if (this.state.updateActive == false) {
      this.setState({ updateActive: true });
      this.requestIntervalId = window.setInterval(this.initiateConnectionStateUpdate,this.state.updateInterval);
    }
  }
  deactivateUpdate() {
    if (this.state.updateActive == true) {
      this.setState({ updateActive: false });
    }
    if (this.requestIntervalId != null) {
      window.clearInterval(this.requestintervalId);
      this.requestIntervalId=null;
    }
  }

  initiateConnectionStateUpdate() {
    if (!this.state.updateActive) { 
      return;
    }
    if (this.requestIsInFlight) { 
      console.log("AJAX: waiting for response from previous reqeust");
      return;
    }
    //console.log("AJAX: initiate request");
    //console.log("request readyState = " + this.request.readyState);
    this.requestIsInFlight = true;
    this.request.open("GET", "/status.json", true);
    this.request.send();
  }

  flushCache() {
    console.log("SmartSOCKSProxy restarted; flush caches");
  }

  handleConnectionStateUpdate() {
    //console.log("request readyState = " + this.request.readyState);
    //console.log("request status " + this.request.status);
    //console.log(this.request.responseText);
    if (this.request.readyState == 4) {
      this.requestIsInFlight = false;
    }
    if (this.request.readyState == 4 && this.request.status == 200) {
      //console.log(this.request);
      if (this.state.updateActive) { 
        let parsedState=JSON.parse(this.request.responseText);
        if (this.proxyStartTime != parsedState.proxyStartTime) { 
          this.flushCache(); // we detected that the server restarted.
          this.proxyStartTime=parsedState.proxyStartTime
        }
        console.log("Updating state... ");
        this.setState({ connectionState: parsedState });
      }
    }
    //if (this.request.readyState == 4) {
    //  this.deactivateUpdate();
    //}
  }

  render() {
    if (!this.state.connectionState.hasOwnProperty('proxyInstance')) {
      return <h3>waiting for response from SmartSocksProxy...</h3>;
    }

    let conState=this.state.connectionState;
    let proxyInst=conState.proxyInstance;
    let sshTunnel=conState.sshTunnel;

    return (
      <div>
        <h1>SmartSocksProxy</h1>
        {Object.values(sshTunnel).map( sshTunnel => {
            return(
              <SshTunnel key={"ssh_"+sshTunnel.name} sshTunnel={sshTunnel} />
          )})}
        <div key="tab_bar" className="tab">
          {Object.values(proxyInst).map( proxyInstance => {
            return(
              <ProxyInstanceTab key={"tablink_"+proxyInstance.name} proxyInstance={proxyInstance} />
          )})}
        </div>
        {Object.values(proxyInst).map( proxyInstance => {
          return(
            <ProxyInstance key={"tab_"+proxyInstance.name} proxyInstance={proxyInstance} />
        )})}
        <p><small><small>Version {conState.version} built on {conState.buildDate} <a href="https://github.com/bryan15/SmartSOCKSProxy">Source</a></small></small></p>
      </div>
    );
  }
}

