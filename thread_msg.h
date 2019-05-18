// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef THREAD_MSG_H
#define THREAD_MSG_H

void thread_msg_init(int fd);
void thread_msg_send(char *msg, int len);

#endif // THREAD_MSG_H
