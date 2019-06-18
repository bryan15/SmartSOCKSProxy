# SmartSOCKSProxy

Routing between network segments is often impaired by NAT or firewalls for security. 
Gaining access to services behind the firewall requires an authenticated tunnel such as SSH.

This works fine until we try to access services in multiple network segments at the same time. It's not
obvious how to route any given connection - should it be routed directly to the IP address,
use SSH as a proxy? Who is responsible for deciding how to route connections, and
who is responsible for starting and stoping SSH sessions?

SmartSOCKSProxy, that's who.

SmartSOCKSProxy is a sock4/5 proxy and SSH manager coupled with a rules engine to intelligently route network connections to where they need to go 
without resorting to static SSH port-forwards. SmartSOCKSProxy allows the client (web browser, java application) to use DNS names,
which may not appear on the local network segment, without resorting to overrides in /etc/hosts (in most cases). 


## Requirements / Setup:

  - Build SmartSOCKSProxy on MacOS: You may need to install xcode?
    - git clone https://github.com/bryan15/SmartSOCKSProxy.git
    - cd SmartSOCKSProxy ; make clean ; make -j4 all
      - the above command should complete without any errors
     
  - Ensure you can ssh into your bastions without a password (if id_rsa is password protected, run “ssh-add ~/.ssh/id_rsa”)

  - Run the unit tests 
    - make test

  - cp ssp.conf.example ssp.conf    # edit as required

  - Test SmartSOCKSProxy by running in the foreground:
    - ./smartsocksproxy -c ssp.conf

  - Copy and edit "run_smartsocksproxy.sh.example"; modify the path to smartsocksproxy you built above.
    Note: this example shell also contains static port-forwarding for access to Dev/QA/Prod Databases. 
    Because SQLDeveloper circumvents JVM SOCKS5 support.  

Running "./smartsocksproxy -h" provides a brief help summary. 

## Build

    $ make -j4 test  # should pass with no errors
    $ make -j4 all

### React

To keep things simple, this project uses React without a build step. To download pre-compiled react components:

    $ make react

References:

* https://medium.com/@chrislewisdev/react-without-npm-babel-or-webpack-1e9a6049714
* https://shinglyu.com/web/2018/02/08/minimal-react-js-without-a-build-step-updated.html

## Configuration

Command-line options and lines in config files executed sequentially, in the order they appear. 
If two or more config lines set the same value, later lines overwrite earlier lines.

### Proxy Instances

SmartSOCKSProxy lets you specify multiple "proxy instances". The primary reason for using multiple
separate "proxy instances" is for debugging. Most of the time you spent struggling with SmartSOCKSProxy
will be spent trying to understand where network connections are going, what's happening, and why. 
It helps to separate the wheat from the chaff, so to speak. 

Therefore, each "proxy instance", created by the "proxy" keyword (see below) in the config file, 
can specify a separate log file and verbosity level. They also appear under separate tabs in the WebUI. 

Each "proxy instance" section has its own set of routing rules. See "Rules" below for rule format. 

### Logfiles

Multiple "proxy instances" can use a single logfile. Or separate logfiles. Whatever you wish. 
You can specify the maximum size of a logfile before the file is rotated, and the number of
previous rotations to keep before deleting the file. 

### SSH

The SSH configuration sectino lets you specify the command to run to bring up the SSH tunnel, and
the SOCKS port the SSH instance will listen on for SOCKS5 connections. See SSH -D option for more information. 

### Config file

Format (in no particular order):

* main
  * logFilename  [ \<filename\> | - ]
  * logVerbosity [ error | warn | info | debug | trace | trace2 ]
* logfile [ \<filename\> | default ]
  * fileRotateCount \<count\>
  * byteCountMax \<max_bytes_before_rotating\>
* ssh [ \<tunnel_name\> | default ]
  * logFilename  [ \<filename\> | - ]
  * logVerbosity [ error | warn | info | debug | trace | trace2 ]
  * socksPort \<SSH_SOCKS_port\>
  * command \<SSH command\>
* proxy [ \<proxy_instance_name\> | default ]
  * logFilename  [ \<filename\> | - ]
  * logVerbosity [ error | warn | info | debug | trace | trace2 ]
  * socksServer [\<bind_address\>:]\<port\>
  * httpServer [\<bind_address\>:]\<port\>:\<html_directory\>
  * portForward [\<bind_address:]\<local_port\>:\<remote_host\>:\<remote_port\>
  * route \<rule\>
  * routeFile \<filename\>
  * routeDir \<dirname\>
  * include \<file\>

"routeFile" will read all rules from the specified file, as though specified with the "route" command in the config file. 

"routeDir" will read all files in the specified directory, sorted alphabetically and parsed in order, and parse them as though they were specified in a "routeFile" command in the config file. 

The config file parser supports primitive environment variable substitution; ${var} will be converted to the value of the environment variable "var" before the line is evaluated.

### Rules 

Each rule consists of three parts: Condition, Permutation and Action.

* Criteria - all criteria must be satisfied for the rule to take effect
  * is \<dns_or_ip\>
  * startsWith \<dns_or_ip\>
  * endsWith \<dns_or_ip\>
  * contains \<dns_or_ip\>
  * port \<port\>
  * network \<ip_or_ip_plus_netmask\>
* Permutation
  * map \<ip_or_ip_plus_netmask\>
  * to \<ip_or_ip_plus_netmask\>
* Action (end-state)
  * via \<ssh_tunnel_name\>
  * resolveDNS

#### Condition

If one or more condition is specified in a rule row, ALL conditions must be met for the rule to apply. 

If there are no conditions in a rule, the rule will always match. 

#### Permutations

Permutations change the hostname or IP address to connect to. 

For example, given the rule: 

    map 10.0.0.10 to 20.0.0.20

when a connection is made to 10.0.0.10, SmartSOCKSProxy will actually connect to 20.0.0.20. Also, subsequent rules will use 20.0.0.20 when evaluating Criteria. 

Other examples:

    map 10.0.0.0/8 to 20.0.0.0/8   # Any address of the form 10.x.y.z will be changed to 20.x.y.z
    

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
 
  - code doesn't need to be a work of art; the project is unlikely to grow large
  - threading is used so the SOCKS5 protocol can be programmed using blocking I/O. This makes the SOCKS5 implementation 
    brittle, but easy to implement and read. 
  - IPv6 is unsupported currently
    - although most of the functionality is there, IPv6 is untested and not implemented in several areas. 

## Development Notes

## MacOS

Useful tools:

  - audit open file handles with "lsof -c smartsocksproxy"
  - "Instruments" is a built-in Mac profiler and resource inspector. Easy to use. Identifies what parts of code are consuming CPU, file handles, etc.

 
## Inspiration: 

  - https://github.com/armon/go-socks5/blob/master/socks5.go
  - https://github.com/isayme/socks5/blob/v2/src/callback.c

