"""Minimal local Chrome DevTools transport; standard library only."""
import base64
import hashlib
import json
import os
import socket
import struct
from urllib.parse import urlsplit


class Connection:
    def __init__(self, url, timeout):
        target = urlsplit(url)
        self.socket = socket.create_connection((target.hostname, target.port), timeout)
        self.socket.settimeout(timeout)
        self.reader = self.socket.makefile('rb')
        self.sequence = 0
        key = base64.b64encode(os.urandom(16)).decode()
        request = (f'GET {target.path} HTTP/1.1\r\nHost: {target.netloc}\r\n'
                   f'Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n'
                   'Sec-WebSocket-Version: 13\r\n\r\n')
        self.socket.sendall(request.encode())
        if b'101' not in self.reader.readline():
            raise RuntimeError('DevTools websocket handshake failed')
        headers = {}
        while True:
            line = self.reader.readline().strip()
            if not line:
                break
            name, value = line.decode().split(':', 1)
            headers[name.lower()] = value.strip()
        expected = base64.b64encode(hashlib.sha1((key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()).decode()
        if headers.get('sec-websocket-accept') != expected:
            raise RuntimeError('Invalid DevTools websocket handshake')

    def send(self, payload, opcode=1):
        payload = payload if isinstance(payload, bytes) else json.dumps(payload).encode()
        size = len(payload)
        header = bytes([0x80 | opcode])
        if size < 126:
            header += bytes([0x80 | size])
        elif size < 65536:
            header += bytes([0x80 | 126]) + struct.pack('!H', size)
        else:
            header += bytes([0x80 | 127]) + struct.pack('!Q', size)
        mask = os.urandom(4)
        self.socket.sendall(header + mask + bytes(value ^ mask[i % 4] for i, value in enumerate(payload)))

    def receive(self):
        message = bytearray()
        while True:
            header = self.reader.read(2)
            if len(header) != 2:
                raise RuntimeError('DevTools connection closed before completion')
            final, opcode, size = header[0] & 0x80, header[0] & 15, header[1] & 127
            if size == 126:
                size = struct.unpack('!H', self.reader.read(2))[0]
            elif size == 127:
                size = struct.unpack('!Q', self.reader.read(8))[0]
            payload = self.reader.read(size)
            if opcode == 8:
                raise RuntimeError('DevTools closed')
            if opcode == 9:
                self.send(payload, 10)
                continue
            message.extend(payload)
            if final:
                return json.loads(message)

    def call(self, method, params=None):
        self.sequence += 1
        self.send({'id': self.sequence, 'method': method, 'params': params or {}})
        while True:
            response = self.receive()
            if response.get('id') == self.sequence:
                if response.get('error'):
                    raise RuntimeError(str(response['error']))
                return response.get('result', {})

    def close(self):
        self.reader.close()
        self.socket.close()
