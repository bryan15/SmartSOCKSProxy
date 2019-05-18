# WARNING
# No makefile magic here. I kept it simple to keep it simple. 
# Fully aware of http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/ 
# and https://stackoverflow.com/questions/297514/how-can-i-have-a-makefile-automatically-rebuild-source-files-that-include-a-modi
# and so on. But... whatever. The cost of simplicity is this: 
#
#    IF YOU CHANGE A HEADER FILE, YOU ***MUST*** "make clean; make"
#
# If you do not do the above, stale .o files might be used, using different struct sizes in memory
# leading to all sorts of wonderful and bizzare problems.  

CC=gcc
CFLAGS=
LDFLAGS=

OBJFILES = main.o log.o string2.o server.o client_connection.o \
        service.o service_http.o service_socks.o service_port_forward.o \
        socks_connection.o build_json.o service_thread.o \
        socks5_client.o shuttle.o safe_close.o \
	safe_blocking_readwrite.o listen_socket.o \
        thread_local.o thread_msg.o proxy_instance.o \
	ssh_tunnel.o ssh_policy.o \
	route_rule.o route_rules_engine.o host_id.o

all: smartsocksproxy

clean: 
	rm -f *.o smartsocksproxy version.h

fail:
	echo start
	git diff-index --quiet HEAD --
	echo end

# run git-diff-index twice; once to print the message, second time to stop the build.
# The '@' suppresses echoing the command to STDOUT
distribution: clean smartsocksproxy
	@git diff-index --quiet HEAD -- || echo "****  Git tree is not clean. Check in your changes before creating a distribution!!! ****"
	@git diff-index --quiet HEAD --
	cp -f smartsocksproxy smartsocksproxy.distribution
	git add smartsocksproxy.distribution
	git commit -m "SmartSOCKSProxy Distribution build"

version.h:
	rm -f version.h
	echo "#define SMARTSOCKSPROXY_VERSION \"`git rev-parse --short HEAD`\"" >> version.h
	echo "#define SMARTSOCKSPROXY_BUILD_DATE \"`date`\"" >> version.h
	echo "#define SMARTSOCKSPROXY_GIT_HASH \"`git rev-parse HEAD`\"" >> version.h

react:
	# Download react components from CDN. This negates the need for a build process on our side
	#wget 'https://unpkg.com/react@16/umd/react.development.js'          -O html/react.js
	#wget 'https://unpkg.com/react-dom@16/umd/react-dom.development.js'  -O html/react-dom.js
	wget 'https://unpkg.com/react@16/umd/react.development.js'          -O html/react.js
	wget 'https://unpkg.com/react-dom@16/umd/react-dom.development.js'  -O html/react-dom.js
	wget 'https://unpkg.com/babel-standalone@6.15.0/babel.min.js'       -O html/babel.min.js

main.o: main.c version.h
	$(CC) $(CFLAGS) -c main.c -o main.o

log.o: log.c
	$(CC) $(CFLAGS) -c log.c -o log.o

string2.o: string2.c
	$(CC) $(CFLAGS) -c string2.c -o string2.o

server.o: server.c
	$(CC) $(CFLAGS) -c server.c -o server.o

client_connection.o: client_connection.c
	$(CC) $(CFLAGS) -c client_connection.c -o client_connection.o

service_socks.o: service_socks.c
	$(CC) $(CFLAGS) -c service_socks.c -o service_socks.o

service_http.o: service_http.c
	$(CC) $(CFLAGS) -c service_http.c -o service_http.o

service_port_forward.o: service_port_forward.c
	$(CC) $(CFLAGS) -c service_port_forward.c -o service_port_forward.o

service_thread.o: service_thread.c
	$(CC) $(CFLAGS) -c service_thread.c -o service_thread.o

socks5_client.o: socks5_client.c
	$(CC) $(CFLAGS) -c socks5_client.c -o socks5_client.o

listen_socket.o: listen_socket.c
	$(CC) $(CFLAGS) -c listen_socket.c -o listen_socket.o

shuttle.o: shuttle.c
	$(CC) $(CFLAGS) -c shuttle.c -o shuttle.o

safe_blocking_readwrite.o: safe_blocking_readwrite.c
	$(CC) $(CFLAGS) -c safe_blocking_readwrite.c -o safe_blocking_readwrite.o

thread_local.o: thread_local.c
	$(CC) $(CFLAGS) -c thread_local.c -o thread_local.o

thread_msg.o: thread_msg.c
	$(CC) $(CFLAGS) -c thread_msg.c -o thread_msg.o

proxy_instance.o: proxy_instance.c
	$(CC) $(CFLAGS) -c proxy_instance.c -o proxy_instance.o

service.o: service.c
	$(CC) $(CFLAGS) -c service.c -o service.o

safe_close.o: safe_close.c
	$(CC) $(CFLAGS) -c safe_close.c -o safe_close.o

socks_connection.o: socks_connection.c
	$(CC) $(CFLAGS) -c socks_connection.c -o socks_connection.o

build_json.o: build_json.c version.h
	$(CC) $(CFLAGS) -c build_json.c -o build_json.o

ssh_tunnel.o: ssh_tunnel.c
	$(CC) $(CFLAGS) -c ssh_tunnel.c -o ssh_tunnel.o

ssh_policy.o: ssh_policy.c
	$(CC) $(CFLAGS) -c ssh_policy.c -o ssh_policy.o

route_rule.o: route_rule.c
	$(CC) $(CFLAGS) -c route_rule.c -o route_rule.o

route_rules_engine.o: route_rules_engine.c
	$(CC) $(CFLAGS) -c route_rules_engine.c -o route_rules_engine.o

host_id.o: host_id.c
	$(CC) $(CFLAGS) -c host_id.c -o host_id.o

smartsocksproxy: $(OBJFILES)
	$(CC) $(LDFLAGS) $(OBJFILES) -o smartsocksproxy

