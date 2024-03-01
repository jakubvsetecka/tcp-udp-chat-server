#include "net.h"

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
    int bytes_to_send = send(sockfd, mail.args[0].c_str(), mail.args[0].size(), 0);
    if (bytes_to_send < 0) {
        std::cerr << "Failed to send data" << std::endl;
    }
}

Mail TcpProtocol::receiveData() {
    Mail mail;
    char buffer[1024] = {0};
    int bytes_received = recv(sockfd, buffer, 1024, 0);
    if (bytes_received < 0) {
        std::cerr << "Failed to receive data" << std::endl;
        mail.type = -1;
        return mail;
    }
    mail.args.push_back(std::string(buffer));
    return mail;
}

//==============================================================================

bool UdpProtocol::createSocket() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
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

void UdpProtocol::sendConfirm(uint16_t seq) {
    Mail mail;
    mail.type = 1; // TODO
    mail.args.push_back(std::to_string(seq));
    sendTo(mail.args[0].c_str(), mail.args[0].size());
}

// checks if the received data is the right confirm
// if not, pipes the data to the spam_mail pipe and send confirm to the server
bool UdpProtocol::getConfirm() {
    Mail mail;

    struct sockaddr_in server_addr; // Ensure this is defined somewhere accessible
    struct sockaddr *addr = (struct sockaddr *)&server_addr;
    socklen_t addrlen = sizeof(server_addr);
    int flags = 0;

    char buffer[1024] = {0};
    int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), flags, addr, &addrlen);

    std::cout << "Received " << bytes_received << " bytes" << std::endl;

    if (bytes_received < 3) {
        std::cerr << "Failed to receive data" << std::endl;
        mail.type = -1; // Consider using a named constant or enum for error codes
        std::cerr << "Failed to receive data, errno: " << errno << std::endl;
    } else {
        mail.args.push_back(std::string(buffer, bytes_received)); // Use bytes_received to include all data
    }
    return true;
}

void UdpProtocol::sendData(const Mail &mail) {

    struct epoll_event ev, events[10];

    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
        perror("epoll_ctl: sockfd");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < retries + 1; i++) {

        sendTo(mail.args[0].c_str(), mail.args[0].size());

        bool waiting = true;
        double milisec_count = this->timeout;

        while (waiting) {
            // start timer
            StopWatch timer;
            int nfds = epoll_wait(epollfd, events, 10, milisec_count);
            // get time
            double duration = timer.duration();

            std::cout << "Duration: " << duration << std::endl;
            std::cout << "milisec_count: " << milisec_count << std::endl;
            std::cout << "nfds: " << nfds << std::endl;

            switch (nfds) {
            case -1:
                perror("epoll_wait");
                exit(EXIT_FAILURE);
            case 0:
                std::cerr << "Timeout" << std::endl;
                waiting = false;
                break;
            default:
                std::cout << "Data received" << std::endl;
                // is it the right data?
                // YES: return
                // NO: continue, set new timer
                if (events[0].data.fd == sockfd) {
                    // receive data
                    bool delete_me_daddy = getConfirm();
                    // check if it's the right data
                    if (mail.type == 1) { // TODO: == 1
                        return;
                    } else {
                        std::cerr << "Received wrong data" << std::endl;
                        milisec_count -= duration; // subtract time passed
                    }
                }
            }
        }
    }
}

Mail UdpProtocol::receiveData() {
    Mail mail;
    char buffer[1024] = {0};
    int bytes_received = recv(sockfd, buffer, 1024, 0);
    if (bytes_received < 0) {
        std::cerr << "Failed to receive data" << std::endl;
        mail.type = -1;
        return mail;
    }
    mail.args.push_back(std::string(buffer));
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
        mail.type = -1;
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