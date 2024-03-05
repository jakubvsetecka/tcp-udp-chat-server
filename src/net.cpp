#include "net.h"
#include "mail-box.h"

bool TcpProtocol::createSocket() {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
    return true;
}

bool TcpProtocol::connectToServer() {
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return false;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        return false;
    }
    return true;
}

TcpProtocol::TcpProtocol(const std::string &ip, const uint16_t &port, const uint16_t &timeout, const uint16_t &retries) {
    this->ip = ip;
    this->port = port;
    this->timeout = timeout;
    this->retries = retries;
}
TcpProtocol::~TcpProtocol() {
    if (sockfd >= 0) {
        close(sockfd);
    }
}
bool TcpProtocol::openConnection() {
    if (!createSocket()) {
        return false;
    }
    if (!connectToServer()) {
        return false;
    }

    std::cout << "Connected successfully to " << ip << " on port " << port << std::endl;
    return true;
}

bool TcpProtocol::closeConnection() {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
    std::cout << "Connection closed" << std::endl;
    return true;
}

void TcpProtocol::sendData(const Mail &mail) {
    // int bytes_to_send = send(sockfd, mail.data, mail.args[0].size(), 0);
    // if (bytes_to_send < 0) {
    //     std::cerr << "Failed to send data" << std::endl;
    // }
}

Mail TcpProtocol::receiveData() {
    Mail mail;
    char buffer[1024] = {0};
    int bytes_received = recv(sockfd, buffer, 1024, 0);
    if (bytes_received < 0) {
        std::cerr << "Failed to receive data" << std::endl;
        // mail.type = -1;
        return mail;
    }
    // mail.args.push_back(std::string(buffer));
    return mail;
}

//==============================================================================

bool UdpProtocol::createSocket() {
    // Create a socket first if not already created
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));

    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on any interface
    client_addr.sin_port = htons(12345);             // Convert port number to network byte order

    // Bind the socket
    if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        return false;
    }

    printMagenta("UDP socket created: " + std::to_string(sockfd) + " on port 12345");

    return true;
}

bool UdpProtocol::connectToServer() {
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return false;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        return false;
    }
    return true;
}

bool UdpProtocol::sendTo(const char *buffer, int size) {
    std::cerr << "Sending data to " << ip << " on port " << port << std::endl;
    int bytes_to_send = sendto(sockfd, buffer, size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bytes_to_send < 0) {
        std::cerr << "Failed to send data" << std::endl;
        return false;
    }
    return true;
}

UdpProtocol::UdpProtocol(const std::string &ip, const uint16_t &port, const uint16_t &timeout, const uint16_t &retries) {
    this->ip = ip;
    this->port = port;
    this->timeout = timeout;
    this->retries = retries;
}
UdpProtocol::~UdpProtocol() {
    if (sockfd >= 0) {
        close(sockfd); // Ensure the socket is closed on destruction
    }
}
bool UdpProtocol::openConnection() {
    if (!createSocket()) {
        return false;
    }
    if (!connectToServer()) {
        return false;
    }

    std::cout << "Connected successfully to " << ip << " on port " << port << std::endl;
    return true;
}

bool UdpProtocol::closeConnection() {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
    std::cout << "Connection closed" << std::endl;
    return true;
}

void UdpProtocol::sendData(const Mail &mail) {
}

Mail UdpProtocol::receiveData() {
    printBlue("Receiving data");
    Mail mail;
    char buffer[1024] = {0};
    int bytes_received = recv(sockfd, buffer, 1024, 0);
    if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available right now, not an error in non-blocking mode
            std::cerr << "No data available to read" << std::endl;
        } else {
            std::cerr << "Failed to receive data: " << strerror(errno) << std::endl;
        }
        return mail;
    } else if (bytes_received == 0) {
        // Handle the case where the remote side has closed the connection
        std::cerr << "Connection closed by peer" << std::endl;
        return mail;
    }
    std::cout << "Received data: " << buffer << std::endl;
    return mail;
}

//==============================================================================

NetworkConnection::NetworkConnection(ProtocolType type, const std::string &ip, const uint16_t &port, const uint16_t &timeout, const uint16_t &retries) {
    if (type == ProtocolType::TCP) {
        protocolPtr = std::make_unique<TcpProtocol>(ip, port, timeout, retries);
    } else if (type == ProtocolType::UDP) {
        protocolPtr = std::make_unique<UdpProtocol>(ip, port, timeout, retries);
    }
}
void NetworkConnection::sendData(const Mail &mail) {
    if (protocolPtr) {
        protocolPtr->sendData(mail);
    }
}
Mail NetworkConnection::receiveData() {
    if (protocolPtr) {
        return protocolPtr->receiveData();
    } else {
        Mail mail;
        // mail.type = -1;
        return mail;
    }
}
bool NetworkConnection::openConnection() {
    if (protocolPtr) {
        return protocolPtr->openConnection();
    } else {
        return false;
    }
}
bool NetworkConnection::closeConnection() {
    if (protocolPtr) {
        return protocolPtr->closeConnection();
    }
    return false;
}
void NetworkConnection::setProtocol(NetworkProtocol *p) {
    protocolPtr.reset(p);
}

int NetworkConnection::getFdsocket() {
    return protocolPtr->getFdsocket();
}