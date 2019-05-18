// Copyright 2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef ROUTE_RULES_ENGINE_H
#define ROUTE_RULES_ENGINE_H

#include"route_rule.h"

int decide_applicable_rule(proxy_instance *proxy, service *srv, client_connection *con);

#endif // ROUTE_RULES_ENGINE_H
