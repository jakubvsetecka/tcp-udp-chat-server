import socket
import struct
import threading

# Server configuration
LISTEN_IP = "127.0.0.1"
LISTEN_PORT = 4567
DEST_IP = "127.0.0.1"
DEST_PORT = 12345  # This will be updated dynamically
USE_TCP = False  # Toggle this to switch between TCP and UDP

# Create a global socket variable
sock = None

def set_protcol(protocol):
    global USE_TCP
    USE_TCP = protocol

def close_socket():
    global sock
    if sock is not None:
        sock.close()
        sock = None

def create_socket():
    global sock
    # Ensure any existing socket is closed before creating a new one
    close_socket()
    if USE_TCP:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(("127.0.0.1", 4567))
        sock.listen(1)
    else:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("127.0.0.1", 4567))
    print(f"{'TCP' if USE_TCP else 'UDP'} server listening on 127.0.0.1:4567")



# Function to send a generic message
def send_message():
    global DEST_PORT, sock
    message = b"Hello, client!"  # Example message for UDP
    if USE_TCP:
        conn, addr = sock.accept()  # Accept new connection
        conn.sendall(message)
        conn.close()
    else:
        if DEST_PORT is not None:
            sock.sendto(message, (DEST_IP, DEST_PORT))
            print(f"Sent message to {DEST_IP}:{DEST_PORT}")

# For TCP, the message formatting and sending logic would be different due to the textual format.
def send_tcp_message(msg_type, **kwargs):
    if msg_type == "ERR":
        message = f"ERR FROM {kwargs['DisplayName']} IS {kwargs['MessageContent']}\r\n"
    elif msg_type == "REPLY":
        result = "OK" if kwargs['Result'] else "NOK"
        message = f"REPLY {result} IS {kwargs['MessageContent']}\r\n"
    elif msg_type == "AUTH":
        message = f"AUTH {kwargs['Username']} AS {kwargs['DisplayName']} USING {kwargs['Secret']}\r\n"
    elif msg_type == "JOIN":
        message = f"JOIN {kwargs['ChannelID']} AS {kwargs['DisplayName']}\r\n"
    elif msg_type == "MSG":
        message = f"MSG FROM {kwargs['DisplayName']} IS {kwargs['MessageContent']}\r\n"
    elif msg_type == "BYE":
        message = "BYE\r\n"
    else:
        message = ""

    if USE_TCP:
        conn, addr = sock.accept()  # Accept new connection
        conn.sendall(message.encode())
        conn.close()

def send_udp_message(msg_type, **kwargs):
    if msg_type == "CONFIRM":
        message = struct.pack('!BH', 0x00, kwargs['ref_message_id'])
    elif msg_type == "REPLY":
        message = struct.pack('!BHBH', 0x01, kwargs['message_id'], kwargs['result'], kwargs['ref_message_id']) + kwargs['message_contents'].encode() + b'\x00'
    elif msg_type == "AUTH":
        message = struct.pack('!BH', 0x02, kwargs['message_id']) + kwargs['username'].encode() + b'\x00' + kwargs['display_name'].encode() + b'\x00' + kwargs['secret'].encode() + b'\x00'
    elif msg_type == "JOIN":
        message = struct.pack('!BH', 0x03, kwargs['message_id']) + kwargs['channel_id'].encode() + b'\x00' + kwargs['display_name'].encode() + b'\x00'
    elif msg_type == "MSG":
        message = struct.pack('!BH', 0x04, kwargs['message_id']) + kwargs['display_name'].encode() + b'\x00' + kwargs['message_contents'].encode() + b'\x00'
    elif msg_type == "ERR":
        message = struct.pack('!BH', 0xFE, kwargs['message_id']) + kwargs['display_name'].encode() + b'\x00' + kwargs['message_contents'].encode() + b'\x00'
    elif msg_type == "BYE":
        message = struct.pack('!BH', 0xFF, kwargs['message_id'])
    else:
        raise ValueError("Unsupported message type")

    sock.sendto(message, (DEST_IP, DEST_PORT))

def send_message(msg_type, **kwargs):
    if USE_TCP:
        send_tcp_message(msg_type, **kwargs)
    else:
        send_udp_message(msg_type, **kwargs)


def listen_for_message():
    global DEST_PORT
    # Set a timeout duration in seconds
    timeout_duration = 5  # For example, 5 seconds

    try:
        if USE_TCP:
            sock.settimeout(timeout_duration)  # Apply timeout to TCP accept
            conn, addr = sock.accept()  # This may raise socket.timeout
            conn.settimeout(timeout_duration)  # Apply timeout to TCP receive
            while True:
                data = conn.recv(1024)
                if not data:
                    break
                print(f"Received message: {data.decode()}")
            conn.close()
        else:
            sock.settimeout(timeout_duration)  # Apply timeout to UDP recvfrom
            data, addr = sock.recvfrom(1024)  # This may raise socket.timeout
            print(f"Received message from {addr[0]}:{addr[1]}")
            if DEST_PORT != addr[1]:
                DEST_PORT = addr[1]
            print(f"Destination port set to {DEST_PORT}")

            if len(data) >= 3:  # Check data length to safely extract refMessageId
                ref_message_id = struct.unpack('!H', data[1:3])[0]
                send_message('CONFIRM', ref_message_id=ref_message_id)
    except socket.timeout:
        print("No data received within timeout period.")

def listen_for_messages():
    while True:
        listen_for_message()

def start_server():
    server_thread = threading.Thread(target=listen_for_messages, daemon=True)
    server_thread.start()
    print("Server started in background thread.")

def end_server():
    close_socket()
    print("Server stopped.")

def main():
    print("Starting server...")
    create_socket()
    while True:
        listen_for_message()

if __name__ == "__main__":
    main()
