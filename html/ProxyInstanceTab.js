// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

class ProxyInstanceTab extends React.Component {
  constructor(props) {
    super(props);
    this.activateTab = this.activateTab.bind(this);
  }

  activateTab(evt) {
    // TODO: port this to React
    let tab = "tab_" + this.props.proxyInstance.name;
    let tablink = "tablink_" + this.props.proxyInstance.name;
 
    // Declare all variables
    let i, tabcontent, tablinks;

    // Get all elements with class="tabcontent" and hide them
    tabcontent = document.getElementsByClassName("tabcontent");
    for (i = 0; i < tabcontent.length; i++) {
      tabcontent[i].style.display = "none";
    }

    // Get all elements with class="tablinks" and remove the class "active"
    tablinks = document.getElementsByClassName("tablinks");
    for (i = 0; i < tablinks.length; i++) {
      tablinks[i].className = tablinks[i].className.replace(" active", "");
    }

    // Show the current tab, and add an "active" class to the button that opened the tab
    document.getElementById(tab).style.display = "block";
    document.getElementById(tablink).className += " active";
  }

  render() {
    let name = this.props.proxyInstance.name;
    return (
      <button className="tablinks" onClick={this.activateTab} id={"tablink_"+name}>{name}</button>
    );
  }
}

