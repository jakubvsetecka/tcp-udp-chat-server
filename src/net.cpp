#include "net.h"
#include "mail-box.h"

bool TcpProtocol::createSocket() {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return false;
    }
    return true;
}

bool TcpProtocol::connectToServer() {
    struct hostent *server = gethostbyname(ip.c_str());
    if (server == NULL) {
        throw std::runtime_error("No such host");
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        throw std::runtime_error("Connection failed");
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

    printGreen("TCP socket created: " + std::to_string(sockfd) + " on port " + std::to_string(port));
    return true;
}

bool TcpProtocol::closeConnection() {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
    printRed("Connection closed");
    return true;
}

void TcpProtocol::sendData(const Mail &mail) {
    MailSerializer serializer(false);
    std::vector<char> buffer = serializer.serialize(mail);
    int bytes_to_send = send(sockfd, buffer.data(), buffer.size(), 0);
    if (bytes_to_send < 0) {
        throw std::runtime_error("Failed to send data");
    }
    printRed("Sending data: " + std::string(buffer.data()) + " to " + ip + " on port " + std::to_string(port));
}

bool TcpProtocol::receiveData(char *buffer) {
    Mail mail;
    int bytes_received = recv(sockfd, buffer, 1024, 0);
    if (bytes_received < 0) {
        throw std::runtime_error("Failed to receive data");
        return false;
    }
    // mail.args.push_back(std::string(buffer));
    return true;
}

//==============================================================================

bool UdpProtocol::createSocket() {

    int family = AF_INET;
    int type = SOCK_DGRAM;
    sockfd = socket(family, type, 0);
    if (sockfd < 0) {
        return false;
    }

    return true;
}

bool UdpProtocol::connectToServer() {
    struct hostent *server = gethostbyname(ip.c_str());
    if (server == NULL) {
        throw std::runtime_error("ERROR: no such host");
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    memcpy(&server_addr.sin_addr.s_addr,
           server->h_addr, server->h_length);

    printGreen("Connected to server: " + ip + " on port " + std::to_string(port) + " with socket " + std::to_string(sockfd) + "\n");

    return true;
}

bool UdpProtocol::sendTo(const char *buffer, int size) {
    printGreen("Sending data to " + ip + " on port " + std::to_string(port));
    int bytes_to_send = sendto(sockfd, buffer, size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bytes_to_send < 0) {
        throw std::runtime_error("Failed to send data");
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

    printGreen("UDP socket created: " + std::to_string(sockfd) + " on port " + std::to_string(port));
    return true;
}

bool UdpProtocol::closeConnection() {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
    printRed("Connection closed");
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
    MailSerializer serializer(true);
    std::vector<char> buffer = serializer.serialize(mail);

    // Convert buffer to a human-readable format for logging, e.g., hex string
    std::string bufferAsString = convertBufferToHexString(buffer);

    printRed("Sending data: " + bufferAsString + " to " + ip + " on port " + std::to_string(port));

    if (sendTo(buffer.data(), buffer.size()) == false) {
        throw std::runtime_error("Failed to send data");
    }
}

bool UdpProtocol::receiveData(char *buffer) {
    struct sockaddr_in fromAddr;
    socklen_t fromAddrLen = sizeof(fromAddr);

    int bytes_received = recvfrom(sockfd, buffer, 1500, 0,
                                  (struct sockaddr *)&fromAddr, &fromAddrLen);
    if (bytes_received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available right now, not an error in non-blocking mode
            std::cerr << "ERR: No data available to read" << std::endl;
        } else {
            throw std::runtime_error("Failed to receive data");
        }
        return false;
    } else if (bytes_received == 0) {
        // Connection closed by peer, which should not occur in UDP
        throw std::runtime_error("Connection closed by peer");
        return false;
    }

    // Extract and print the sender's port number
    unsigned int senderPort = ntohs(fromAddr.sin_port);
    printWhite("Received data: " + std::string(buffer) + " from " + ip + " on port " + std::to_string(port));

    if (senderPort != port) {
        port = senderPort;
        printWhite("Port changed to: " + std::to_string(port));
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