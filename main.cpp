#include "arg-parser.h"
#include "listeners.h"
#include "net.h"
#include "pipes.h"
#include <iostream>

int main(int argc, char **argv) {

    // Step 1: Create a Pipe object for StdinListener.
    Pipe myPipe("StdinPipe"); // Assume Pipe constructor takes a name or similar identifier.

    // Step 2: Get file descriptors from the Pipe object. (For demonstration, we're directly using myPipe here)
    int readFd = myPipe.getReadFd(); // Assuming Pipe class has getReadFd method.

    Pipe ToSendPipe("ToSendPipe");                   // Create a Pipe instance named ToSendPipe
    MailBox mailbox(ProtocolType::UDP, &ToSendPipe); // Create a MailBox instance

    // Instantiate Listener with fds
    Listener myListener(&mailbox);       // Instantiate the Listener object
    myListener.addFd(readFd, StdinPipe); // Assuming Listener class has addFd method.

    // Step 4: Instantiate StdinListener with the address of myPipe.
    StdinListener myStdinListener(&myPipe);

    // Example interaction: Wait for user to press enter, then exit.
    std::cout << "Press enter to exit..." << std::endl;
    std::cin.get();

    // Step 5: Destroy the StdinListener and Listener objects.

    //    ArgumentParser args(argc, argv);
    //
    //    NetworkConnection connection(args.type, args.ip, args.port, args.timeout, args.retries);
    //    connection.openConnection();
    //
    //    std::thread listener();
    //
    //    Mail mail;
    //    mail.type = 1;
    //    mail.args.push_back("Hello, World!\n");
    //    connection.sendData(mail);
    //
    //    connection.closeConnection();

    return 0;
}
