// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef SOCKS5_H
#define SOCKS5_H

#define SOCKS5_AUTH_NONE_REQUIRED          0x00
#define SOCKS5_AUTH_GSSAPI                 0x01
#define SOCKS5_AUTH_USERNAME_PASSWORD      0x02
#define SOCKS5_AUTH_NO_ACCEPTABLE_METHODS  0xFF

#define SOCKS5_CMD_ERROR          0x00
#define SOCKS5_CMD_CONNECT        0x01
#define SOCKS5_CMD_BIND           0x02
#define SOCKS5_CMD_UDP_ASSOCIATE  0x03

#define SOCKS5_ADDRTYPE_IPV4      0x01
#define SOCKS5_ADDRTYPE_DOMAIN    0x03
#define SOCKS5_ADDRTYPE_IPV6      0x04

#define SOCKS5_REPLY_SUCCEED                     0x00
#define SOCKS5_REPLY_SERVER_FAILURE              0x01
#define SOCKS5_REPLY_NOT_ALLOWED                 0x02
#define SOCKS5_REPLY_NETWORK_UNREACHABLE         0x03
#define SOCKS5_REPLY_HOST_UNREACHABLE            0x04
#define SOCKS5_REPLY_CONNECTION_REFUSED          0x05
#define SOCKS5_REPLY_TTL_EXPIRED                 0x06
#define SOCKS5_REPLY_COMMAND_NOT_SUPPORTED       0x07
#define SOCKS5_REPLY_ADDRESS_TYPE_NOT_SUPPORTED  0x08


#endif // SOCKS5_H
