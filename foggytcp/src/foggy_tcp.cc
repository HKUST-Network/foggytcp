/* Copyright (C) 2024 Hong Kong University of Science and Technology

This repository is used for the Computer Networks (ELEC 3120) 
course taught at Hong Kong University of Science and Technology. 

No part of the project may be copied and/or distributed without 
the express permission of the course staff. Everyone is prohibited 
from releasing their forks in any public places. */
 
 /*
 * This file implements the high-level API for foggy-TCP sockets.
 */

#include "foggy_tcp.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "foggy_backend.h"

void* foggy_socket(const foggy_socket_type_t socket_type,
               const char *server_port, const char *server_ip) {
  foggy_socket_t* sock = new foggy_socket_t;
  int sockfd, optval;
  socklen_t len;
  struct sockaddr_in conn, my_addr;
  len = sizeof(my_addr);

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("ERROR opening socket");
    return NULL;
  }
  sock->socket = sockfd;
  // sock->state = CLOSED;
  sock->received_buf = NULL;
  sock->received_len = 0;
  pthread_mutex_init(&(sock->recv_lock), NULL);

  sock->sending_buf = NULL;
  sock->sending_len = 0;
  pthread_mutex_init(&(sock->send_lock), NULL);

  sock->type = socket_type;
  sock->dying = 0;
  sock->peer_set = socket_type == TCP_INITIATOR;
  sock->peer_closed = 0;
  sock->fin_sent = 0;
  sock->fin_acked = 0;
  pthread_mutex_init(&(sock->death_lock), NULL);

  // FIXME: Sequence numbers should be randomly initialized. The next expected
  // sequence number should be initialized according to the SYN packet from the
  // other side of the connection.
  sock->window.last_byte_sent = 0;
  sock->window.last_ack_received = 0;
  sock->window.dup_ack_count = 0;
  sock->window.next_seq_expected = 0;
  sock->window.ssthresh = WINDOW_INITIAL_SSTHRESH;
  sock->window.advertised_window = WINDOW_INITIAL_ADVERTISED;
  sock->window.congestion_window = WINDOW_INITIAL_WINDOW_SIZE;
  sock->window.reno_state = RENO_SLOW_START;
  pthread_mutex_init(&(sock->window.ack_lock), NULL);

  for (int i = 0; i < RECEIVE_WINDOW_SLOT_SIZE; ++i) {
    sock->receive_window[i].is_used = 0;
    sock->receive_window[i].msg = NULL;
  }

  if (pthread_cond_init(&sock->wait_cond, NULL) != 0) {
    perror("ERROR condition variable not set\n");
    return NULL;
  }

  uint16_t portno = (uint16_t)atoi(server_port);
  switch (socket_type) {
    case TCP_INITIATOR:
      if (server_ip == NULL) {
        perror("ERROR server_ip NULL");
        return NULL;
      }
      memset(&conn, 0, sizeof(conn));
      
      conn.sin_family = AF_INET;
      conn.sin_addr.s_addr = inet_addr(server_ip);
      conn.sin_port = htons(portno);
      sock->conn = conn;

      my_addr.sin_family = AF_INET;
      my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
      my_addr.sin_port = 0;
      if (::bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        perror("ERROR on binding");
        return NULL;
      }

      break;

    case TCP_LISTENER:
      memset(&conn, 0, sizeof(conn));
      conn.sin_family = AF_INET;
      conn.sin_addr.s_addr = htonl(INADDR_ANY);
      conn.sin_port = htons(portno);

      optval = 1;
      setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
                 sizeof(int));
      if (::bind(sockfd, (struct sockaddr *)&conn, sizeof(conn)) < 0) {
        perror("ERROR on binding");
        return NULL;
      }
      sock->conn = conn;
      break;

    default:
      perror("Unknown Flag");
      return NULL;
  }
  getsockname(sockfd, (struct sockaddr *)&my_addr, &len);
  sock->my_port = ntohs(my_addr.sin_port);

  pthread_create(&(sock->thread_id), NULL, begin_backend, (void *)sock);
  return (void*)sock;
}

int foggy_close(void *in_sock) {
  auto *sock = static_cast<foggy_socket_t *>(in_sock);
  if (!sock) return EXIT_ERROR;
  pthread_mutex_lock(&sock->death_lock);
  sock->dying = 1;
  pthread_mutex_unlock(&sock->death_lock);
  pthread_join(sock->thread_id, NULL);

  // The owner has stopped application reads/writes before releasing the handle.
  int result = sock->peer_set && !sock->fin_acked ? EXIT_ERROR : EXIT_SUCCESS;
  if (close(sock->socket) != 0) result = EXIT_ERROR;
  free(sock->received_buf);
  free(sock->sending_buf);
  for (auto &slot : sock->send_window) free(slot.msg);
  for (auto &slot : sock->receive_window) if (slot.is_used) free(slot.msg);
  pthread_mutex_destroy(&sock->recv_lock);
  pthread_mutex_destroy(&sock->send_lock);
  pthread_mutex_destroy(&sock->death_lock);
  pthread_mutex_destroy(&sock->window.ack_lock);
  pthread_cond_destroy(&sock->wait_cond);
  delete sock;
  return result;
}

int foggy_read(void* in_sock, void *buf, int length) {
  struct foggy_socket_t *sock = (struct foggy_socket_t *)in_sock;  
  uint8_t *new_buf;
  int read_len = 0;

  if (length == 0) return 0;
  if (length < 0) {
    perror("ERROR negative length");
    return EXIT_ERROR;
  }

  while (pthread_mutex_lock(&(sock->recv_lock)) != 0) {
  }

  while (sock->received_len == 0 && !sock->peer_closed) {
    pthread_cond_wait(&(sock->wait_cond), &(sock->recv_lock));
  }
  if (sock->received_len > 0) {
    if (sock->received_len > length)
      read_len = length;
    else
      read_len = sock->received_len;

    memcpy(buf, sock->received_buf, read_len);
    if (read_len < sock->received_len) {
      new_buf = (uint8_t*) malloc(sock->received_len - read_len);
      memcpy(new_buf, sock->received_buf + read_len,
              sock->received_len - read_len);
      free(sock->received_buf);
      sock->received_len -= read_len;
      sock->received_buf = new_buf;
    } else {
      free(sock->received_buf);
      sock->received_buf = NULL;
      sock->received_len = 0;
    }
  }
  pthread_mutex_unlock(&(sock->recv_lock));
  return read_len;
}

int foggy_write(void *in_sock, const void *buf, int length) {
  struct foggy_socket_t *sock = (struct foggy_socket_t *)in_sock;
  while (pthread_mutex_lock(&(sock->send_lock)) != 0) {
  }
  if (sock->sending_buf == NULL)
    sock->sending_buf = (uint8_t*) malloc(length);
  else
    sock->sending_buf = (uint8_t*) realloc(sock->sending_buf, length + sock->sending_len);
  memcpy(sock->sending_buf + sock->sending_len, buf, length);
  sock->sending_len += length;

  pthread_mutex_unlock(&(sock->send_lock));
  return EXIT_SUCCESS;
}
