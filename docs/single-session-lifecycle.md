# Single-session lifecycle repair

This branch keeps the existing `foggy_socket`, `foggy_read`, `foggy_write`, and
`foggy_close` declarations unchanged. It retains stop-and-wait and the existing
packet layout. Socket state gains internal fields; rebuild consumers rather than
assuming binary ABI compatibility.

A listener claims its first valid sequence-zero data packet and pins that source
IP/UDP port for the handle's lifetime. Other sources and stray initial ACK/FIN
packets cannot replace its peer or contaminate its receive state.

`foggy_close` drains queued data before sending FIN, retries unacknowledged FIN,
and frees the socket and its state after the backend exits. Receiving in-order
FIN wakes readers: buffered data is returned first, then a positive-length
`foggy_read` returns zero. EOF does not call `foggy_close` or stop local writes;
the application decides when it is finished with the handle. To serve another
session, close the old handle and create a fresh listener at the same port.

Unacknowledged stop-and-wait data is retried after 100 ms. Only ACKs covering the
packet end release it. Duplicate or unexpected-sequence packets do not occupy
the single receive slot; the current cumulative ACK is returned.

## Validation

On Linux with g++ and Python 3, from the repository root:

```sh
python3 -m unittest discover -s tests -p test_single_session.py -v
```

The test harness routes existing transport diagnostics to stderr and exercises
the original four functions: drain-before-EOF, foreign-peer isolation, three
sequential sessions on the same port, and closing an unclaimed listener.
No HTTP implementation or instructor HTTP solution is included in this branch.

The same four patched transport files were tested locally on 2026-10-29 in the
course's CP3 HTTP reference environment, with client/server network namespaces:
CP3 2.3 passed 20/20, CP3 3.1 passed 10/10, and the complete suite passed 100/100.
These results validate those scenarios, not all possible TCP behavior.

## Defined limits

- One active peer per handle; no accept API, connection multiplexing, SYN
  handshake, random initial sequence numbers, or TIME_WAIT.
- No shutdown-write API: `foggy_close` releases the whole local handle, so its
  caller cannot continue reading a response after it returns. Receiving EOF
  does not automatically close the receiving endpoint, but a peer which has
  completed close may already have stopped receiving. Delayed half-close
  request/reply behavior is not guaranteed by this minimal repair.
- Close waits at most about two seconds; if FIN was not acknowledged it returns
  an error and still releases local resources. It may discard unacknowledged
  queued data. FIN retries use 100 ms intervals and a 200 ms acknowledgment
  linger, not a complete production TCP closing state machine.
- Arbitrary packet loss/delay, rapid four-tuple reuse, and concurrent
  application read/write versus handle destruction are not validated.
