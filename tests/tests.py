import subprocess
import unittest
import server
import time

class TestFSM(unittest.TestCase):
    binary_path = '../bin/main'  # Path to your C++ compiled binary

    def start_client(self, args=None):
        # Start the client binary with the provided arguments as a list
        command = [self.binary_path] + (args if args else [])
        # Open a subprocess with the given command
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        return process

    def tearDown(self):
        # Kill any running client processes
        subprocess.run(['pkill', '-f', self.binary_path])
        # End server
        server.end_server()

    def set_protocol(self, protocol):
        # Set the protocol for the server
        server.USE_TCP = protocol == 'TCP'
        # Re-initialize the socket after changing the protocol
        server.create_socket()

    def test_udp_start_to_auth(self):
        # Simulate sending AUTH message from start state and verify output
        self.set_protocol('UDP')
        server.start_server()
        process = self.start_client(args=['-t', 'udp', '-s', 'localhost', '-d', '250', '-p', '4567', '-v'])
        time.sleep(1)  # Wait for the server to be ready
        process.stdin.write('/auth a b c\n')
        process.stdin.flush()  # Ensure the message is sent

        # Read the outputs
        stdout, stderr = process.communicate()
        returncode = process.returncode

        self.assertIn('Expected response', stderr)
        self.assertEqual(returncode, 0)


if __name__ == '__main__':
    unittest.main()
