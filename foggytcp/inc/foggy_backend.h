
/* Copyright (C) 2024 Hong Kong University of Science and Technology

This repository is used for the Computer Networks (ELEC 3120) 
course taught at Hong Kong University of Science and Technology. 

No part of the project may be copied and/or distributed without 
the express permission of the course staff. Everyone is prohibited 
from releasing their forks in any public places. */


/* This file defines the function signatures for the foggy-TCP backend that should
be exposed. The backend runs in a different thread and handles all the socket
operations separately from the application. */
 

#ifndef BACKEND_H_
#define BACKEND_H_

#include "foggy_tcp.h"

/**
 * Launches the Foggy-TCP backend.
 *
 * @param in the socket to be used for backend processing.
 */
void* begin_backend(void* in);


int has_been_acked(foggy_socket_t *sock, uint32_t seq);

/**
 * Checks if the socket received any data.
 *
 * It first peeks at the header to figure out the length of the packet and
 * then reads the entire packet.
 *
 * @param sock The socket used for receiving data on the connection.
 * @param flags Flags that determine how the socket should wait for data.
 * Check `foggy_read_mode_t` for more information.
 */
void check_for_pkt(foggy_socket_t *sock, foggy_read_mode_t flags);

#endif  // BACKEND_H_
