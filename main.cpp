#include "arg-parser.h"
#include "fsm.h"
#include "listeners.h"
#include "net.h"
#include "pipes.h"
#include <iostream>

int main(int argc, char **argv) {

    ArgumentParser args(argc, argv);

    NetworkConnection connection(args.type, args.ip, args.port, args.timeout, args.retries);
    connection.openConnection();
    int sockfd = connection.getFdsocket();

    // Step 1: Create a Pipe object for StdinListener.
    Pipe myPipe(fdType::StdinPipe); // Assume Pipe constructor takes a name or similar identifier.

    // Step 2: Get file descriptors from the Pipe object. (For demonstration, we're directly using myPipe here)
    int readFd = myPipe.getReadFd(); // Assuming Pipe class has getReadFd method.

    Pipe toSendPipe(fdType::ToSendPipe);             // Create a Pipe instance named ToSendPipe
    int readMailFd = toSendPipe.getReadFd();         // Get the read file descriptor of ToSendPipe
    MailBox mailbox(ProtocolType::UDP, &toSendPipe); // Create a MailBox instance

    // Instantiate Listener with fds
    Listener myListener(&mailbox, &connection, args.type); // Instantiate the Listener object
    myListener.addFd(readFd, StdinPipe);                   // STDIN
    myListener.addFd(sockfd, SocketPipe);                  // Socket
    myListener.addFd(readMailFd, ToSendPipe);              // MailBox
    myListener.run();                                      // Start the listener thread

    // Step 4: Instantiate StdinListener with the address of myPipe.
    StdinListener myStdinListener(&myPipe);

    FSM fsm(State::START, mailbox);
    fsm.run();

    // Step 5: Wait for the listener thread to complete.
    // while (1) {
    //}

    connection.closeConnection();

    return 0;
}
