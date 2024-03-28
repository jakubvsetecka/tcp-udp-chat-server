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

TcpProtocol::TcpProtocol(const std::string &ip, const uint16_t &port) {
    this->ip = ip;
    this->port = port;
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

bool TcpProtocol::receiveData(char *buffer) {
    Mail mail;
    int bytes_received = recv(sockfd, buffer, 1024, 0);
    if (bytes_received < 0) {
        std::cerr << "Failed to receive data" << std::endl;
        // mail.type = -1;
        return false;
    }
    // mail.args.push_back(std::string(buffer));
    return true;
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

    // if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    //     std::cerr << "Connection Failed" << std::endl;
    //     return false;
    // }
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

std::string UdpProtocol::convertBufferToHexString(const std::vector<char> &buffer) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char ch : buffer) {
        ss << std::setw(2) << static_cast<int>(ch);
    }
    return ss.str();
}

void UdpProtocol::sendData(const Mail &mail) {
    MailSerializer serializer;
    std::vector<char> buffer = serializer.serialize(mail);

    // Convert buffer to a human-readable format for logging, e.g., hex string
    std::string bufferAsString = convertBufferToHexString(buffer);

    printRed("Sending data: " + bufferAsString + " to " + ip + " on port " + std::to_string(port));

    ssize_t sentBytes = sendto(sockfd, buffer.data(), buffer.size(), MSG_CONFIRM,
                               (const struct sockaddr *)&server_addr, sizeof(server_addr));

    if (sentBytes == -1) {
        std::cerr << "Failed to send data: " << strerror(errno) << std::endl;
    }
}

bool UdpProtocol::receiveData(char *buffer) {
    struct sockaddr_in fromAddr;
    socklen_t fromAddrLen = sizeof(fromAddr);

    int bytes_received = recvfrom(sockfd, buffer, 1024, 0,
                                  (struct sockaddr *)&fromAddr, &fromAddrLen);
    if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available right now, not an error in non-blocking mode
            std::cerr << "No data available to read" << std::endl;
        } else {
            std::cerr << "Failed to receive data: " << strerror(errno) << std::endl;
        }
        return false;
    } else if (bytes_received == 0) {
        // Connection closed by peer, which should not occur in UDP
        std::cerr << "Connection closed by peer" << std::endl;
        return false;
    }

    // Extract and print the sender's port number
    unsigned int senderPort = ntohs(fromAddr.sin_port);
    std::cout << "Received packet from port: " << senderPort << std::endl;

    if (senderPort != port) {
        port = senderPort;
        std::cout << "Port changed to: " << port << std::endl;
        this->connectToServer();
    }

    return true;
}

//==============================================================================

NetworkConnection::NetworkConnection(ProtocolType type, const std::string &ip, const uint16_t &port, const uint16_t &timeout, const uint16_t &retries) {
    if (type == ProtocolType::TCP) {
        protocolPtr = std::make_unique<TcpProtocol>(ip, port);
    } else if (type == ProtocolType::UDP) {
        protocolPtr = std::make_unique<UdpProtocol>(ip, port, timeout, retries);
    }
}
void NetworkConnection::sendData(const Mail &mail) {
    if (protocolPtr) {
        protocolPtr->sendData(mail);
    }
}
bool NetworkConnection::receiveData(char *buffer) {
    if (protocolPtr) {
        return protocolPtr->receiveData(buffer);
    } else {
        Mail mail;
        // mail.type = -1;
        return true;
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