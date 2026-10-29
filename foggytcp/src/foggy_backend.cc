/* Copyright (C) 2024 Hong Kong University of Science and Technology

This repository is used for the Computer Networks (ELEC 3120) 
course taught at Hong Kong University of Science and Technology. 

No part of the project may be copied and/or distributed without 
the express permission of the course staff. Everyone is prohibited 
from releasing their forks in any public places. */
 
 /*
 * This file implements the foggy-TCP backend. The backend runs in a different
 * thread and handles all the socket operations separately from the application.
 *
 * This is where most of your code should go. Feel free to modify any function
 * in this file.
 */



#include <assert.h>
#include <chrono>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "foggy_backend.h"
#include "foggy_function.h"
#include "foggy_packet.h"
#include "foggy_tcp.h"

/**
 * Tells if a given sequence number has been acknowledged by the socket.
 *
 * @param sock The socket to check for acknowledgements.
 * @param seq Sequence number to check.
 *
 * @return 1 if the sequence number has been acknowledged, 0 otherwise.
 */
int has_been_acked(foggy_socket_t *sock, uint32_t seq) {
  int result;
  while (pthread_mutex_lock(&(sock->window.ack_lock)) != 0) {
  }
  result = after(sock->window.last_ack_received, seq);
  pthread_mutex_unlock(&(sock->window.ack_lock));
  return result;
}

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
void check_for_pkt(foggy_socket_t *sock, foggy_read_mode_t flags) {
  uint8_t packet[MAX_LEN];
  sockaddr_in source{};
  socklen_t length = sizeof(source);
  ssize_t n = recvfrom(sock->socket, packet, sizeof(packet),
                       flags == NO_WAIT ? MSG_DONTWAIT : 0,
                       reinterpret_cast<sockaddr *>(&source), &length);
  if (n < static_cast<ssize_t>(sizeof(foggy_tcp_header_t))) return;
  auto *header = reinterpret_cast<foggy_tcp_header_t *>(packet);
  if (ntohl(header->identifier) != IDENTIFIER ||
      get_hlen(header) < sizeof(foggy_tcp_header_t) ||
      get_hlen(header) > get_plen(header) || get_plen(header) != n) return;

  pthread_mutex_lock(&sock->recv_lock);
  if (!sock->peer_set) {
    // Bind only on the first data packet, never on a stale ACK or FIN.
    if (get_plen(header) == get_hlen(header) || get_seq(header) != 0 ||
        (get_flags(header) & FIN_FLAG_MASK)) {
      pthread_mutex_unlock(&sock->recv_lock);
      return;
    }
    sock->conn = source;
    sock->peer_set = 1;
  } else if (source.sin_addr.s_addr != sock->conn.sin_addr.s_addr ||
             source.sin_port != sock->conn.sin_port) {
    fprintf(stderr, "IGNORED_PEER %u\n", ntohs(source.sin_port));
    pthread_mutex_unlock(&sock->recv_lock);
    return;
  }
  on_recv_pkt(sock, packet);
  if (sock->received_len > 0 || sock->peer_closed)
    pthread_cond_broadcast(&sock->wait_cond);
  pthread_mutex_unlock(&sock->recv_lock);
}

static void send_fin(foggy_socket_t *sock) {
  auto *packet = create_packet(sock->my_port, ntohs(sock->conn.sin_port),
      sock->window.last_byte_sent, sock->window.next_seq_expected,
      sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t),
      FIN_FLAG_MASK | ACK_FLAG_MASK, MAX_NETWORK_BUFFER, 0, NULL, NULL, 0);
  if (!packet) return;
  sock->fin_sent = 1;
  sendto(sock->socket, packet, sizeof(foggy_tcp_header_t), 0,
         reinterpret_cast<sockaddr *>(&sock->conn), sizeof(sock->conn));
  free(packet);
}

void *begin_backend(void *in) {
  auto *sock = static_cast<foggy_socket_t *>(in);
  using Clock = std::chrono::steady_clock;
  auto closing = Clock::time_point{};
  auto last_fin = Clock::time_point{};
  auto acknowledged = Clock::time_point{};
  for (;;) {
    pthread_mutex_lock(&sock->death_lock);
    bool death = sock->dying;
    pthread_mutex_unlock(&sock->death_lock);
    auto now = Clock::now();
    if (death && closing == Clock::time_point{}) closing = now;
    if (death && now - closing >= std::chrono::seconds(2)) break;

    pthread_mutex_lock(&sock->send_lock);
    int count = sock->sending_len;
    uint8_t *data = sock->sending_buf;
    sock->sending_len = 0;
    sock->sending_buf = NULL;
    pthread_mutex_unlock(&sock->send_lock);
    // The backend alone owns the send/receive windows.
    send_pkts(sock, data, count);
    free(data);
    check_for_pkt(sock, NO_WAIT);

    if (death && count == 0 && sock->send_window.empty()) {
      if (!sock->peer_set) break;
      now = Clock::now();
      if (!sock->fin_acked &&
          (last_fin == Clock::time_point{} || now - last_fin >= std::chrono::milliseconds(100))) {
        send_fin(sock);
        last_fin = now;
      }
      if (sock->fin_acked) {
        if (acknowledged == Clock::time_point{}) acknowledged = now;
        // Briefly keep ACKing the peer's FIN, including retransmissions.
        if (now - acknowledged >= std::chrono::milliseconds(200)) break;
      }
    }
    usleep(1000);
  }
  return NULL;
}
