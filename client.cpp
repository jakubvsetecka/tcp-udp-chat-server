#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h> // For inet_addr
#include <unistd.h>    // For close

// TODO output a short notice like this when it starts in an interactive mode:
/*
<program>  Copyright (C) <year>  <name of author>
    This program comes with ABSOLUTELY NO WARRANTY; for details type `show w'.
    This is free software, and you are welcome to redistribute it
    under certain conditions; type `show c' for details.
*/

int main()
{
    int socket_desc;
    struct sockaddr_in server;
    char *message = "Hello, Server!\n";

    // Create socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1)
    {
        std::cerr << "Could not create socket";
        return 1;
    }

    server.sin_addr.s_addr = inet_addr("127.0.0.1"); // Server IP address
    server.sin_family = AF_INET;
    server.sin_port = htons(8888); // Server port

    // Connect to remote server
    if (connect(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        std::cerr << "Connect error";
        return 1;
    }

    std::cout << "Connected\n";

    // Send some data
    if (send(socket_desc, message, strlen(message), 0) < 0)
    {
        std::cerr << "Send failed";
        return 1;
    }

    std::cout << "Data Sent\n";

    close(socket_desc); // Properly close the socket

    return 0;
}