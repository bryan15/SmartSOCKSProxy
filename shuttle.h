#ifndef SHUTTLE_H
#define SHUTTLE_H

#include"client_connection.h"

int shuttle_data_back_and_forth(client_connection *con);
int shuttle_null_connection(client_connection *con);

#endif // SHUTTLE_H
