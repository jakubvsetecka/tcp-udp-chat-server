import socket
import time

# clietn configuration
DEST_IP = "127.0.0.1"
DEST_PORT = 12345
MESSAGE = b"Hello, UDP!"

# server configuration
SRC_IP = "127.0.0.1"
SRC_PORT = 4567

# create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Bind the socket to the specified IP address and port
sock.bind((SRC_IP, SRC_PORT))

while True:
    sock.sendto(MESSAGE, (DEST_IP, DEST_PORT))
    time.sleep(5)  # wait for 5 seconds