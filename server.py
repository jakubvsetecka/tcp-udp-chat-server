import socket
import struct
from tkinter import *
from tkinter import simpledialog

# Client and server configuration
DEST_IP = "127.0.0.1"
DEST_PORT = 12345
SRC_IP = "127.0.0.1"
SRC_PORT = 4567

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((SRC_IP, SRC_PORT))

# Function to send CONFIRM message
def send_confirm():
    ref_message_id = simpledialog.askinteger("Input", "Ref Message ID:")
    if ref_message_id is not None:
        message = struct.pack('!BH', 0x00, ref_message_id)
        sock.sendto(message, (DEST_IP, DEST_PORT))

# Function to send REPLY message
def send_reply():
    message_id = simpledialog.askinteger("Input", "Message ID:")
    result = simpledialog.askinteger("Input", "Result (0 or 1):")
    ref_message_id = simpledialog.askinteger("Input", "Ref Message ID:")
    message_contents = simpledialog.askstring("Input", "Message Contents:")
    if None not in (message_id, result, ref_message_id, message_contents):
        message = struct.pack('!BHBH', 0x01, message_id, result, ref_message_id) + message_contents.encode() + b'\x00'
        sock.sendto(message, (DEST_IP, DEST_PORT))

# Function to send AUTH message
def send_auth():
    message_id = simpledialog.askinteger("Input", "Message ID:")
    username = simpledialog.askstring("Input", "Username:")
    display_name = simpledialog.askstring("Input", "Display Name:")
    secret = simpledialog.askstring("Input", "Secret:")
    if None not in (message_id, username, display_name, secret):
        message = struct.pack('!BH', 0x02, message_id) + username.encode() + b'\x00' + display_name.encode() + b'\x00' + secret.encode() + b'\x00'
        sock.sendto(message, (DEST_IP, DEST_PORT))

# Function to send JOIN message
def send_join():
    message_id = simpledialog.askinteger("Input", "Message ID:")
    channel_id = simpledialog.askstring("Input", "Channel ID:")
    display_name = simpledialog.askstring("Input", "Display Name:")
    if None not in (message_id, channel_id, display_name):
        message = struct.pack('!BH', 0x03, message_id) + channel_id.encode() + b'\x00' + display_name.encode() + b'\x00'
        sock.sendto(message, (DEST_IP, DEST_PORT))

# Function to send MSG message
def send_msg():
    message_id = simpledialog.askinteger("Input", "Message ID:")
    display_name = simpledialog.askstring("Input", "Display Name:")
    message_contents = simpledialog.askstring("Input", "Message Contents:")
    if None not in (message_id, display_name, message_contents):
        message = struct.pack('!BH', 0x04, message_id) + display_name.encode() + b'\x00' + message_contents.encode() + b'\x00'
        sock.sendto(message, (DEST_IP, DEST_PORT))

# Function to send ERR message
def send_err():
    message_id = simpledialog.askinteger("Input", "Message ID:")
    display_name = simpledialog.askstring("Input", "Display Name:")
    message_contents = simpledialog.askstring("Input", "Message Contents:")
    if None not in (message_id, display_name, message_contents):
        message = struct.pack('!BH', 0xFE, message_id) + display_name.encode() + b'\x00' + message_contents.encode() + b'\x00'
        sock.sendto(message, (DEST_IP, DEST_PORT))

# Function to send BYE message
def send_bye():
    message_id = simpledialog.askinteger("Input", "Message ID:")
    if message_id is not None:
        message = struct.pack('!BH', 0xFF, message_id)
        sock.sendto(message, (DEST_IP, DEST_PORT))


# Create the main window
root = Tk()
root.title("Packet Sender")

# Add buttons for each message type
Button(root, text="CONFIRM", command=send_confirm).pack(fill=X)
Button(root, text="REPLY", command=send_reply).pack(fill=X)
Button(root, text="AUTH", command=send_auth).pack(fill=X)
Button(root, text="JOIN", command=send_join).pack(fill=X)
Button(root, text="MSG", command=send_msg).pack(fill=X)
Button(root, text="ERR", command=send_err).pack(fill=X)
Button(root, text="BYE", command=send_bye).pack(fill=X)

root.mainloop()
