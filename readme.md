# SmartSOCKSProxy

Routing between network segments is often impaired by NAT or a firewall for security. 
Gaining access to services behind the firewall requires an authenticated tunnel such as VPN or SSH.

This works fine until we try to access services in multiple network segments at the same time. It's not
obvious how to route any given connection - should it be routed directly to the IP address,
go through a VPN, or use SSH as a proxy? Who is responsible for deciding how to route connections, and
who is responsible for starting and stoping SSH sessions?

SmartSOCKSProxy, that's who.

SmartSOCKSProxy is a sock4/5 proxy and SSH manager coupled with a rules engine to intelligently route network connections to where they need to go 
without resorting to static SSH port-forwards. SmartSOCKSProxy allows the client (web browser, java application) to use DNS names,
which may not appear on the local network segment, without resorting to overrides in /etc/hosts (in most cases). 


## Requirements / Setup:

  - Build SmartSOCKSProxy on MacOS: You may need to install xcode?
    - cd smartsocksproxy ; make clean ; make -j4 all
    - the above command should complete without any errors

  - You can ssh into your bastions without a password (if id_rsa is password protected, run “ssh-add ~/.ssh/id_rsa”)

  - Run the unit tests 
    - make test

  - Test SmartSOCKSProxy by running in the foreground:
    - ./smartsocksproxy

  - Copy and edit "run_smartsocksproxy.sh.example"; modify the path to smartsocksproxy you built above.
    Note: this example shell also contains static port-forwarding for access to Dev/QA/Prod Databases. 
    Because SQLDeveloper circumvents JVM SOCKS5 support.  

Running "./smartsocksproxy -h" provides a brief help summary. 
## Build

### React

To keep things simple, this project uses React without a build step. To download pre-compiled react components:

    make react

References:

 - https://medium.com/@chrislewisdev/react-without-npm-babel-or-webpack-1e9a6049714
 - https://shinglyu.com/web/2018/02/08/minimal-react-js-without-a-build-step-updated.html

## Configuration


Command-line options and lines in a config file executed sequentially, in the order they appear. 
If two or more config lines set the same value, later lines overwrite earlier lines.
IE: last line wins. 

### Peculiarities

The main thread is "special" with respect to logging. Two command-line options let you setup 
main-thread verbosity and filename. The settings take effect immediately; command-line options
are parsed and executed in the order they appear on the command line. 

IE: If you want to increase verbosity or capture output of config file parsing, 
use -v and -V options *before* the -c option on the command-line.


## Configure Your Browser or Application:

### Firefox

Preferences → Advanced → Network → Connection (Configure how Firefox connects...) Settings → Manual Proxy Configuration

*or*

Preferences → &lt;search for "socks"&gt; → Settings → Manual Proxy Configuration

Set:

  - SOCKS Host = 127.0.0.1
  - SOCKS Port = 11080
  - select SOCKS v5
  - No Proxy For: &lt;leave blank&gt;
    - SmartSOCKSProxy will handle connections to 127.0.0.1 just fine. 
  - Select "Proxy DNS when using SOCKS v5"

![Firefox Proxy Setup](doc/FirefoxProxySetup.png "Firefox Proxy Setup")

### Chrome 

  - Settings -&gt; &lt;search for "proxy"&gt; -&gt; Open proxy settings
  - continue for MacOS below
  
### MacOS

  - Open Network Settings
  - Select "Proxies" Tab
  - Select "SOCKS Proxy". Additional setting should appear to the right. 
  - Socks Proxy Server = 127.0.0.1 : 11080

![MacOS Proxy Setup](doc/MacOSProxySetup.png "MacOS Proxy Setup")

### Java Applications

Add the following options to the JVM command-line: 

    -DsocksProxyHost=127.0.0.1 -DsocksProxyPort=11080

![IntelliJ Proxy Setup](doc/IntelliJProxySetup.png "IntelliJ Proxy Setup")

### /etc/hosts

SmartSOCKSProxy eliminates most requirements for /etc/hosts overrides. 

However, there is one case where SmartSOCKSProxy needs help: if you try to reach a DNS name which does not exist in the local-configured DNS, but the application 
resolves the DNS to an IP before opening the connection, it will fail on DNS resolution. IE: This all happens before SmartSOCKSProxy is 
contacted, so there's nothing SmartSOCKSProxy can do about it.

To avoid this, we augment the local DNS with the missing DNS entries, or add them to /etec/hosts.

## Scenarios / Use Cases

SmartSOCKSProxy is best understood through its use cases. 

### Connect to IP Address, Direct

If the IP address is directly addressible from the local network segment, connect to the IP address directly. 

### Connect to IP Address, Remote Segment

If the IP address resides within a remote segmented network: 

  - Open SSH connection to the remote's bastion, configured as a SOCKS5 server
  - SmartSOCKSProxy, acting as a SOCK5 client, routes the connection through SSH to the destination

## Design Goals

### SmartSOCKSProxy Design Goals: 

  - fast
  - stable
    - must never crash
    - handles all edge cases / error conditions elegantly, if not perfectly 
  - loggging
  - routing policy consolidated in a single place
    - easy to inspect and change

### SmartSOCKSProxy Non-Goals: 
 
  - code doesn't need to be perfect; the project shouldn't grow very large
  - threading is used so the SOCKS5 protocol can be programmed using blocking I/O. This makes the SOCKS5 implementation 
    brittle, but easy to implement and read. 
  - IPv6 is unsupported
    - although most of the functionality is there, IPv6 is untested and not implemented in several areas. 


 
## Inspiration: 

  - https://github.com/armon/go-socks5/blob/master/socks5.go
  - https://github.com/isayme/socks5/blob/v2/src/callback.c

