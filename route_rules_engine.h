#ifndef ROUTE_RULES_ENGINE_H
#define ROUTE_RULES_ENGINE_H

#include"route_rule.h"

int decide_applicable_rule(proxy_instance *proxy, service *srv, client_connection *con);

#endif // ROUTE_RULES_ENGINE_H
