#!/usr/bin/env python3
"""Linux integration tests using only foggy_socket/read/write/close."""
import os
from pathlib import Path
import select
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]

class Peer:
    def __init__(self, binary, listener=False, port=19089):
        self.log = tempfile.TemporaryFile()
        self.process = subprocess.Popen(
            [str(binary), 'listen' if listener else 'connect', '127.0.0.1', str(port)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log)
        self.pending = b''
        if not self.line().startswith('READY '):
            self.stop()
            raise AssertionError('peer did not start')
    def line(self, timeout=5):
        deadline = time.monotonic() + timeout
        while b'\n' not in self.pending:
            left = deadline-time.monotonic()
            if left <= 0 or not select.select([self.process.stdout], [], [], max(0,left))[0]:
                self.log.seek(0)
                raise AssertionError('control timeout: ' + self.log.read().decode(errors='replace'))
            data = os.read(self.process.stdout.fileno(), 4096)
            if not data: raise AssertionError('peer exited unexpectedly')
            self.pending += data
        data, self.pending = self.pending.split(b'\n', 1)
        return data.decode()
    def command(self, text):
        self.process.stdin.write((text+'\n').encode()); self.process.stdin.flush()
        return self.line()
    def send(self, data):
        if self.command('SEND '+data.hex()) != 'SENT': raise AssertionError('send failed')
    def read(self, count=4096):
        return self.command('READ '+str(count))
    def start_close(self):
        self.process.stdin.write(b'CLOSE\n'); self.process.stdin.flush()
    def stop(self):
        if self.process.poll() is None:
            self.process.terminate()
            try: self.process.wait(timeout=3)
            except subprocess.TimeoutExpired: self.process.kill(); self.process.wait()
        self.process.stdin.close(); self.process.stdout.close(); self.log.close()

class SingleSessionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='foggy-session-tests-')
        cls.binary = Path(cls.build.name)/'peer'
        sources = [ROOT/'foggytcp/src'/name for name in
                   ('foggy_tcp.cc','foggy_backend.cc','foggy_function.cc','foggy_packet.cc')]
        subprocess.run(['g++','-std=c++17','-O1','-g','-pthread',
                        '-fno-builtin-printf','-fno-builtin-fprintf',
                        '-I'+str(ROOT/'foggytcp/inc'),
                        *map(str,sources), str(ROOT/'tests/session_api_peer.cpp'),
                        str(ROOT/'tests/stdio_redirect.cpp'),
                        '-Wl,--wrap=printf,--wrap=fprintf','-o',str(cls.binary)], check=True)
    @classmethod
    def tearDownClass(cls): cls.build.cleanup()
    def peer(self, listener=False):
        peer = Peer(self.binary, listener)
        self.addCleanup(peer.stop)
        return peer
    def close_pair(self, client, server):
        client.start_close()
        self.assertEqual(server.read(), 'EOF')
        server.start_close()
        self.assertEqual(client.line(), 'CLOSED 0')
        self.assertEqual(server.line(), 'CLOSED 0')
        self.assertEqual(client.process.wait(timeout=3), 0)
        self.assertEqual(server.process.wait(timeout=3), 0)
    def test_buffer_drained_before_eof(self):
        server, client = self.peer(True), self.peer()
        client.send(b'ABC\x00XYZ'); client.start_close()
        self.assertEqual(server.read(2), 'DATA 4142')
        self.assertEqual(server.read(), 'DATA 430058595a')
        self.assertEqual(server.read(), 'EOF')
        server.start_close()
        self.assertEqual(client.line(), 'CLOSED 0')
        self.assertEqual(server.line(), 'CLOSED 0')
        client.process.wait(timeout=3); server.process.wait(timeout=3)
    def test_foreign_peer_cannot_change_owner(self):
        server, owner = self.peer(True), self.peer()
        owner.send(b'owner')
        self.assertEqual(server.read(), 'DATA '+b'owner'.hex())
        intruder = self.peer(); intruder.send(b'foreign')
        # Allow several foreign-packet retransmissions while the owner is active.
        time.sleep(.25)
        owner.send(b'tail')
        self.assertEqual(server.read(), 'DATA '+b'tail'.hex())
        server.send(b'reply')
        self.assertEqual(owner.read(), 'DATA '+b'reply'.hex())
        self.close_pair(owner, server)
    def test_same_port_three_sequential_sessions(self):
        for index in range(3):
            server, client = self.peer(True), self.peer()
            data = f'session-{index}'.encode(); client.send(data)
            self.assertEqual(server.read(), 'DATA '+data.hex())
            self.close_pair(client, server)
    def test_unclaimed_listener_closes(self):
        server = self.peer(True); server.start_close()
        self.assertEqual(server.line(), 'CLOSED 0')
        self.assertEqual(server.process.wait(timeout=3), 0)

if __name__ == '__main__': unittest.main()
