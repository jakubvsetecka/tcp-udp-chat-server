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

    // Create a vector of fds for Listener. We'll listen to the read end of myPipe.
    std::vector<int> fds{readFd};

    // Step 3: Instantiate Listener with fds.
    Listener myListener(fds);

    // Step 4: Instantiate StdinListener with the address of myPipe.
    StdinListener myStdinListener(&myPipe);

    // Example interaction: Wait for user to press enter, then exit.
    std::cout << "Press enter to exit..." << std::endl;
    std::cin.get();

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
