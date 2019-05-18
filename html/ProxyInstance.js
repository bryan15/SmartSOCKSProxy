class ProxyInstance extends React.Component {
  constructor(props) {
    super(props);
  }

  render() {
    let key="tab_"+this.props.proxyInstance.name;
    return (
      <div>
        <div key={key} id={key} className="tabcontent">
          {Object.values(this.props.proxyInstance.service).map( service => {
            return(
              <Service key={"Service_"+service.serviceId} service={service} />
          )})}
          {Object.values(this.props.proxyInstance.connection).map( connection => {
            return(
              <Connection key={"Connection_"+connection.connectionId} connection={connection} service={this.props.proxyInstance.service[connection.service]} />
          )})}
        </div>
      </div>
    );
  }
}

