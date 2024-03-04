import socket
import time

# server configuration
IP = "127.0.0.1"
PORT = 12345
MESSAGE = b"Hello, UDP!"

# create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

while True:
    sock.sendto(MESSAGE, (IP, PORT))
    time.sleep(5)  # wait for 5 seconds
