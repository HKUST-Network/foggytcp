/* Copyright (C) 2024 Hong Kong University of Science and Technology

This repository is used for the Computer Networks (ELEC 3120) 
course taught at Hong Kong University of Science and Technology. 

No part of the project may be copied and/or distributed without 
the express permission of the course staff. Everyone is prohibited 
from releasing their forks in any public places. */
 
/* This file defines the API for the Foggy TCP implementation.
 */

#ifndef FOGGY_TCP_H_
#define FOGGY_TCP_H_

#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <deque>

#include "foggy_packet.h"
#include "grading.h"

using namespace std;

#define EXIT_SUCCESS 0
#define EXIT_ERROR -1
#define EXIT_FAILURE 1

/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
#define RECEIVE_WINDOW_SLOT_SIZE 64

typedef enum {
  RENO_SLOW_START = 0,
  RENO_CONGESTION_AVOIDANCE = 1,
  RENO_FAST_RECOVERY = 2,
} reno_state_t;

typedef struct {
  int is_sent;
  uint8_t* msg;

  int is_rtt_sample;
  struct timespec send_time;
  time_t timeout_interval;
} send_window_slot_t;

typedef struct {
  uint8_t* msg;
  int is_used;
} receive_window_slot_t;

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */

typedef enum {
  TCP_INITIATOR = 0,
  TCP_LISTENER = 1,
} foggy_socket_type_t;

typedef struct {
  uint32_t last_byte_sent;
  uint32_t last_ack_received;
  
  uint32_t dup_ack_count;
  uint32_t next_seq_expected;

  uint32_t ssthresh;
  uint32_t advertised_window;
  uint32_t congestion_window;

  reno_state_t reno_state;
  pthread_mutex_t ack_lock;
} window_t;

/**
 * This structure holds the state of a socket. You may modify this structure as
 * you see fit to include any additional state you need for your implementation.
 */
struct foggy_socket_t {
  int socket;
  // foggy_tcp_state_t state;
  pthread_t thread_id;
  uint16_t my_port;
  struct sockaddr_in conn;
  uint8_t* received_buf;
  int received_len;
  pthread_mutex_t recv_lock;
  pthread_cond_t wait_cond;
  uint8_t* sending_buf;
  int sending_len;
  foggy_socket_type_t type;
  pthread_mutex_t send_lock;
  // Internal single-session state; the four public API declarations are unchanged.
  int peer_set;
  int peer_closed;
  int fin_sent;
  int fin_acked;
  int dying;
  pthread_mutex_t death_lock;
  window_t window;

  /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
  deque<send_window_slot_t> send_window;
  receive_window_slot_t receive_window[RECEIVE_WINDOW_SLOT_SIZE];
  /* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
};

/*
 * DO NOT CHANGE THE DECLARATIONS BELOW
 */

/**
 * Read mode flags supported by a FoggyTCP socket.
 */
typedef enum {
  NO_FLAG = 0,  // Default behavior: block indefinitely until data is available.
  NO_WAIT,      // Return immediately if no data is available.
  TIMEOUT,      // Block until data is available or the timeout is reached.
} foggy_read_mode_t;

/**
 * Constructs a FoggyTCP socket.
 *
 * An Initiator socket is used to connect to a Listener socket.
 *
 * @param sock The structure with the socket state. It will be initialized by
 *             this function.
 * @param socket_type Indicates the type of socket: Listener or Initiator.
 * @param port Port to either connect to, or bind to. (Based on socket_type.)
 * @param server_ip IP address of the server to connect to. (Only used if the
 *                 socket is an initiator.)
 *
 * @return 0 on success, -1 on error.
 */
void* foggy_socket(const foggy_socket_type_t socket_type,
               const char* port, const char* server_ip);

/**
 * Closes a CMU-TCP socket.
 *
 * @param sock The socket to close.
 *
 * @return 0 on success, -1 on error.
 */
int foggy_close(void* sock);

/**
 * Reads data from a CMU-TCP socket.
 *
 * If there is data available in the socket buffer, it is placed in the
 * destination buffer.
 *
 * @param sock The socket to read from.
 * @param buf The buffer to read into.
 * @param length The maximum number of bytes to read.
 * @param flags Flags that determine how the socket should wait for data. Check
 *             `foggy_read_mode_t` for more information. `TIMEOUT` is not
 *             implemented for CMU-TCP.
 *
 * @return The number of bytes read on success, -1 on error.
 */
int foggy_read(void* sock, void* buf, const int length);

/**
 * Writes data to a CMU-TCP socket.
 *
 * @param sock The socket to write to.
 * @param buf The data to write.
 * @param length The number of bytes to write.
 *
 * @return 0 on success, -1 on error.
 */
int foggy_write(void* sock, const void* buf, int length);

/*
 * You can declare more functions after this point if you need to.
 */

#endif  // FOGGY_TCP_H_
