// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef SHUTTLE_H
#define SHUTTLE_H

#include"client_connection.h"

int shuttle_data_back_and_forth(client_connection *con);
int shuttle_null_connection(client_connection *con);

#endif // SHUTTLE_H
