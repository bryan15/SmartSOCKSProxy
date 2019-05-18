#ifndef SERVICE_THREAD_H
#define SERVICE_THREAD_H

#include"client_connection.h"

void service_thread_setup(void *data);
void service_thread_shutdown(client_connection *con, int okay);

#endif // SERVICE_THREAD_H
