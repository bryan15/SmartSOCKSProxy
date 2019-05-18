// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef SAFE_CLOSE_H
#define SAFE_CLOSE_H

#include"client_connection.h"

void safe_close(client_connection *con, int fd);

#endif // SAFE_CLOSE_H
