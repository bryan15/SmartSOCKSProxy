// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

let selectedTab = null;
if (window.location.hash) {
  selectedTab = window.location.hash.replace('#', '');
}

ReactDOM.render(
  <SmartSOCKSProxy selectedTab={selectedTab} />,
  document.getElementById('root')
);

