#include "arg-parser.h"
#include "listeners.h"
#include "net.h"
#include "pipes.h"
#include <iostream>

int main(int argc, char **argv) {

    ArgumentParser args(argc, argv);

    NetworkConnection connection(args.type, args.ip, args.port, args.timeout, args.retries);
    int sockfd;
    connection.openConnection(sockfd);

    std::cout << "Socket created: " << sockfd << std::endl;

    // Step 1: Create a Pipe object for StdinListener.
    Pipe myPipe(fdType::StdinPipe); // Assume Pipe constructor takes a name or similar identifier.

    // Step 2: Get file descriptors from the Pipe object. (For demonstration, we're directly using myPipe here)
    int readFd = myPipe.getReadFd(); // Assuming Pipe class has getReadFd method.

    Pipe ToSendPipe(fdType::ToSendPipe);             // Create a Pipe instance named ToSendPipe
    MailBox mailbox(ProtocolType::UDP, &ToSendPipe); // Create a MailBox instance

    // Instantiate Listener with fds
    Listener myListener(&mailbox, &connection); // Instantiate the Listener object
    myListener.addFd(readFd, StdinPipe);        // Assuming Listener class has addFd method.
    myListener.addFd(sockfd, SocketPipe);

    // Step 4: Instantiate StdinListener with the address of myPipe.
    StdinListener myStdinListener(&myPipe);

    // Step 5: Wait for the listener thread to complete.
    while (1) {
        // ... do something ...
    }

    connection.closeConnection();

    return 0;
}
