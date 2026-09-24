import socket
import ssl
import base64
import os
import struct

def test_ws(path):
    host = "stream.binance.com"
    port = 9443
    
    # TCP connection
    sock = socket.create_connection((host, port))
    
    # TLS wrapper
    context = ssl.create_default_context()
    ssock = context.wrap_socket(sock, server_hostname=host)
    
    # WS Handshake
    key = base64.b64encode(os.urandom(16)).decode('utf-8')
    req = (f"GET {path} HTTP/1.1\r\n"
           f"Host: {host}\r\n"
           f"Upgrade: websocket\r\n"
           f"Connection: Upgrade\r\n"
           f"Sec-WebSocket-Key: {key}\r\n"
           f"Sec-WebSocket-Version: 13\r\n"
           f"\r\n")
    ssock.sendall(req.encode())
    
    resp = ssock.recv(4096)
    print(f"Handshake response for {path}:", resp.split(b'\r\n')[0])
    
    # Wait for frames
    ssock.settimeout(5.0)
    try:
        frame = ssock.recv(2)
        if frame:
            print(f"Received frame header: {frame.hex()}")
        else:
            print("Connection closed.")
    except socket.timeout:
        print("Timeout waiting for frames.")

test_ws("/ws/btcusdt@depth")
test_ws("/ws/btcusdt@trade")
