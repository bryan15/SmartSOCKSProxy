class Service extends React.Component {
  constructor(props) {
    super(props);
  }

  render() {
    let service = this.props.service;
    if (service.type == "SOCKS") {
      return (
        <div>Service {service.serviceId} {service.type} listening on {service.localAddress}:{service.localPort}</div>
      );
    } else if (service.type == "portForward") {
      return (
        <div>Service {service.serviceId} {service.type} {service.localAddress}:{service.localPort} &rarr; {service.remoteAddress}:{service.remotePort}</div>
      );
    } else if (service.type == "HTTP") {
      return (
        <div>Service {service.serviceId} {service.type} listening on {service.localAddress}:{service.localPort} baseDir {service.baseDir}</div>
      );
    } else {
      return (
        <div>Service {service.serviceId} {service.type}</div>
      );
    }
  }
}


