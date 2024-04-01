import subprocess
import unittest
import server
import time
import signal

protocol = None

class TestFSMUDP(unittest.TestCase):
    binary_path = '../bin/main'  # Path to your C++ compiled binary
    process = None

    def setUp(self):
        # Start the server before each test
        server.start_server()
        self.process = self.start_client(args=['-t', 'udp', '-s', 'localhost', '-d', '250', '-p', '4567'])

    def tearDown(self):

        # Kill any running client processes
        subprocess.run(['pkill', '-f', self.binary_path])

        # End server
        server.end_server()

        # Terminate the process gracefully
        self.process.terminate()
        self.process.wait()  # Wait for the process to terminate

        # Close any open file handles associated with the process
        if self.process.stdout:
            self.process.stdout.close()
        if self.process.stderr:
            self.process.stderr.close()
        if self.process.stdin:
            self.process.stdin.close()

        # Set the process to None
        self.process = None

    def start_client(self, args=None):
        # Start the client binary with the provided arguments as a list
        command = [self.binary_path] + (args if args else [])
        # Open a subprocess with the given command
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        return process

    def simulate_server_reply(self, msg_type, **kwargs):
        """
        Simulates a server reply by putting a message in the message queue.
        The server's thread should pick this up and send it to the client.
        """
        server.enqueue_message(msg_type, **kwargs)

    #==================TESTS==================

    def test_udp_auth_to_open_transition(self):
        self.process.stdin.write('/auth user pass secret\n')
        self.process.stdin.flush()
        time.sleep(1)  # Simulate server response time
        self.simulate_server_reply('REPLY', message_id=1, result=1, message_contents='Authenticated successfully', display_name='user', ref_message_id=1)
        stdout, _ = self.process.communicate()
        self.assertIn('Authenticated successfully', stdout)





if __name__ == '__main__':
    protocol = 'UDP'
    unittest.main()
