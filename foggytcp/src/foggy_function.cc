/* Copyright (C) 2024 Hong Kong University of Science and Technology

This repository is used for the Computer Networks (ELEC 3120) 
course taught at Hong Kong University of Science and Technology. 

No part of the project may be copied and/or distributed without 
the express permission of the course staff. Everyone is prohibited 
from releasing their forks in any public places. */

#include <deque>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "foggy_function.h"
#include "foggy_backend.h"


#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

#define DEBUG_PRINT 1
#define debug_printf(fmt, ...)                            \
  do {                                                    \
    if (DEBUG_PRINT) fprintf(stdout, fmt, ##__VA_ARGS__); \
  } while (0)


/**
 * Updates the socket information to represent the newly received packet.
 *
 * In the current stop-and-wait implementation, this function also sends an
 * acknowledgement for the packet.
 *
 * @param sock The socket used for handling packets received.
 * @param pkt The packet data received by the socket.
 */
void on_recv_pkt(foggy_socket_t *sock, uint8_t *pkt) {
  debug_printf("Received packet\n");
  foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)pkt;
  uint8_t flags = get_flags(hdr);

  if (flags & FIN_FLAG_MASK) {
    // FIN follows the last data byte and consumes one sequence number.
    if (get_payload_len(pkt) == 0 && get_seq(hdr) == sock->window.next_seq_expected) {
      sock->peer_closed = 1;
      uint8_t *ack = create_packet(sock->my_port, ntohs(sock->conn.sin_port),
          sock->window.last_byte_sent, sock->window.next_seq_expected + 1,
          sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t), ACK_FLAG_MASK,
          MAX_NETWORK_BUFFER, 0, NULL, NULL, 0);
      if (ack) {
        sendto(sock->socket, ack, sizeof(foggy_tcp_header_t), 0,
               reinterpret_cast<sockaddr *>(&sock->conn), sizeof(sock->conn));
        free(ack);
      }
      pthread_cond_broadcast(&sock->wait_cond);
    }
    return;
  }


  switch (flags) {
    case ACK_FLAG_MASK: {
      uint32_t ack = get_ack(hdr);
      printf("Receive ACK %d\n", ack);

      // if (get_payload_len(pkt) == 0) handle_congestion_window(sock, pkt);
      sock->window.advertised_window = get_advertised_window(hdr);

      if (sock->fin_sent && ack == sock->window.last_byte_sent + 1)
        sock->fin_acked = 1;
      if (!after(ack, sock->window.last_byte_sent) &&
          after(ack, sock->window.last_ack_received)) {
        sock->window.last_ack_received = ack;
      }
    }

    // Fallthrough.
    default: {
      if (get_payload_len(pkt) > 0 && !sock->peer_closed) {
        debug_printf("Received data packet %d %d\n", get_seq(hdr),
                     get_seq(hdr) + get_payload_len(pkt));

        sock->window.advertised_window = get_advertised_window(hdr);
        // Add the packet to receive window and process receive window
        add_receive_window(sock, pkt);
        process_receive_window(sock);
        // Send ACK
        debug_printf("Sending ACK packet %d\n", sock->window.next_seq_expected);

        uint8_t *ack_pkt = create_packet(
            sock->my_port, ntohs(sock->conn.sin_port),
            sock->window.last_byte_sent, sock->window.next_seq_expected,
            sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t), ACK_FLAG_MASK,
            MAX(MAX_NETWORK_BUFFER - (uint32_t)sock->received_len, MSS), 0,
            NULL, NULL, 0);
        sendto(sock->socket, ack_pkt, sizeof(foggy_tcp_header_t), 0,
               (struct sockaddr *)&(sock->conn), sizeof(sock->conn));
        free(ack_pkt);
      }
    }
  }
}

/**
 * Breaks up the data into packets and sends a single packet at a time.
 *
 * You should most certainly update this function in your implementation.
 *
 * @param sock The socket to use for sending data.
 * @param data The data to be sent.
 * @param buf_len The length of the data being sent.
 */
void send_pkts(foggy_socket_t *sock, uint8_t *data, int buf_len) {
  uint8_t *data_offset = data;
  transmit_send_window(sock);

  if (buf_len > 0) {
    while (buf_len != 0) {
      uint16_t payload_len = MIN(buf_len, (int)MSS);

      send_window_slot_t slot{};
      slot.is_sent = 0;
      slot.msg = create_packet(
          sock->my_port, ntohs(sock->conn.sin_port),
          sock->window.last_byte_sent, sock->window.next_seq_expected,
          sizeof(foggy_tcp_header_t), sizeof(foggy_tcp_header_t) + payload_len,
          ACK_FLAG_MASK,
          MAX(MAX_NETWORK_BUFFER - (uint32_t)sock->received_len, MSS), 0, NULL,
          data_offset, payload_len);
      sock->send_window.push_back(slot);

      buf_len -= payload_len;
      data_offset += payload_len;
      sock->window.last_byte_sent += payload_len;
    }
  }
  receive_send_window(sock);
}


void add_receive_window(foggy_socket_t *sock, uint8_t *pkt) {
  foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)pkt;
  // Stop-and-wait: duplicate/out-of-order data must not occupy the sole slot.
  if (get_seq(hdr) != sock->window.next_seq_expected) return;

  // Stop-and-wait implementation
  // Insert the packet into the first place of receive window
  receive_window_slot_t *cur_slot = &(sock->receive_window[0]);
  if (cur_slot->is_used == 0) {
    cur_slot->is_used = 1;
    cur_slot->msg = (uint8_t*) malloc(get_plen(hdr));
    memcpy(cur_slot->msg, pkt, get_plen(hdr));
  }
}

void process_receive_window(foggy_socket_t *sock) {
  // Stop-and-wait implementation.
  // Only process the first packet in the window.
  receive_window_slot_t *cur_slot = &(sock->receive_window[0]);
  if (cur_slot->is_used != 0) {
    foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)cur_slot->msg;
    // Discard unexpected packet
    if (get_seq(hdr) != sock->window.next_seq_expected) return;
    // Update next seq number expected
    uint16_t payload_len = get_payload_len(cur_slot->msg);
    sock->window.next_seq_expected += payload_len;
    // Copy to received_buf
    sock->received_buf = (uint8_t*)
        realloc(sock->received_buf, sock->received_len + payload_len);
    memcpy(sock->received_buf + sock->received_len, get_payload(cur_slot->msg),
           payload_len);
    sock->received_len += payload_len;
    // Free the slot
    cur_slot->is_used = 0;
    free(cur_slot->msg);
    cur_slot->msg = NULL;
  }
}

void transmit_send_window(foggy_socket_t *sock) {
  if (sock->send_window.empty()) return;

  // An stop-and-wait implementation. 
  // For the first slot:
  // If it has not been sent, send it.
  // If it has been sent but not ACKed, skip.
  send_window_slot_t& slot = sock->send_window.front();
  foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)slot.msg;
  if (slot.is_sent) {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t elapsed = (now.tv_sec - slot.send_time.tv_sec) * 1000 +
                      (now.tv_nsec - slot.send_time.tv_nsec) / 1000000;
    if (elapsed < slot.timeout_interval) return;
  }
  if (sendto(sock->socket, slot.msg, get_plen(hdr), 0,
             reinterpret_cast<sockaddr *>(&sock->conn), sizeof(sock->conn)) >= 0) {
    slot.is_sent = 1;
    slot.timeout_interval = 100;
    clock_gettime(CLOCK_MONOTONIC, &slot.send_time);
  }
}

void receive_send_window(foggy_socket_t *sock) {
  // Pop out the packets that have been ACKed
  while (1) {
    if (sock->send_window.empty()) break;

    send_window_slot_t slot = sock->send_window.front();
    foggy_tcp_header_t *hdr = (foggy_tcp_header_t *)slot.msg;

    if (slot.is_sent == 0) {
      break;
    }
    pthread_mutex_lock(&sock->window.ack_lock);
    uint32_t ack = sock->window.last_ack_received;
    pthread_mutex_unlock(&sock->window.ack_lock);
    // Release only after the entire packet is acknowledged.
    if (before(ack, get_seq(hdr) + get_payload_len(slot.msg))) {
      break;
    }
    sock->send_window.pop_front();
    free(slot.msg);
  }
}
