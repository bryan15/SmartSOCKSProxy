let selectedTab = null;
if (window.location.hash) {
  selectedTab = window.location.hash.replace('#', '');
}

ReactDOM.render(
  <SmartSOCKSProxy selectedTab={selectedTab} />,
  document.getElementById('root')
);

