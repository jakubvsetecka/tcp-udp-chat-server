#include "arg-parser.h"
#include "fsm.h"
#include "listeners.h"
#include "net.h"
#include "pipes.h"
#include <iostream>

#include <csignal>
#include <iostream>
#include <unistd.h> // For sleep()

int writeSignalFd;

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

    Pipe signalPipe(fdType::SignalPipe);   // Create a Pipe instance named SignalPipe
    int signalFd = signalPipe.getReadFd(); // singalFd is a global variable
    writeSignalFd = signalPipe.getWriteFd();

    // Instantiate Listener with fds
    Listener myListener(&mailbox, &connection, args.type); // Instantiate the Listener object
    myListener.addFd(readFd, StdinPipe);                   // STDIN
    myListener.addFd(sockfd, SocketPipe);                  // Socket
    myListener.addFd(readMailFd, ToSendPipe);              // MailBox
    myListener.addFd(signalFd, SignalPipe);                // SignalPipe
    myListener.run();                                      // Start the listener thread

    // Step 4: Instantiate StdinListener with the address of myPipe.
    StdinListener myStdinListener(&myPipe);

    // Register signal SIGTERM and signal handler
    signal(SIGINT, Listener::handleSignal);

    FSM fsm(State::START, mailbox);
    fsm.run();

    std::cout << "Main thread is running" << std::endl;

    myStdinListener.stop(); // Stop the listener thread
    myListener.stop();      // Stop the listener thread

    // Step 5: Wait for the listener thread to complete.
    // while (1) {
    //}

    connection.closeConnection();

    return 0;
}
