// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef SOCKS4_H
#define SOCKS4_H

#include"socks5.h"

#define SOCKS4_CMD_CONNECT                    SOCKS5_CMD_CONNECT

#define SOCKS4_CD_REQUEST_GRANTED             90
#define SOCKS4_CD_REQUEST_REJECTED_OR_FAILED  91
#define SOCKS4_CD_IDENTD_CONNECTION_REFUSED   92
#define SOCKS4_CD_IDENTD_DIFFERENT_USER_IDS   93

#endif // SOCKS4_H
