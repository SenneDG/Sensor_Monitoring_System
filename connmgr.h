//
// Created by senne on 12/12/22.
//

#ifndef SENNE_CONNMGR_H
#define SENNE_CONNMGR_H

#include "sbuffer.h"

#ifndef MAX_CONN
#define MAX_CONN 3
#endif

#ifndef TIMEOUT
#error set TIMEOUT not set
#endif

/**
 * Creates a server and listend on a specified port number.
 * Listens to TCP connections in a multithreaded way and writes the receives sensor_data_t in an sbuffer.
 * \param port the port where the server will listen on.
 * \param sbuffer the shared buffer where the server will write the data in.
 * */
void connmgr_listen(int port, sbuffer_t *sbuffer);

/**
 * frees all memory occupied by the connection manager
 * */
void connmgr_free();

#endif //SENNE_CONNMGR_H
