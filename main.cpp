#include <memory> // For std::unique_ptr
#include <string>
#include <unistd.h> // Depending on your needs, this might be used for sleep, etc.
#include <vector>

#include <arpa/inet.h> // For inet_pton
#include <cstring>     // For memset
#include <iostream>
#include <netinet/in.h> // For sockaddr_in
#include <sys/socket.h> // For socket functions

enum class ParamRegex {
    Username = 0,
    ChannelID,
    Secret,
    DisplayName,
    MessageContent
};

const std::string regexPatterns[] = {
    "^[A-Za-z0-9-]{1,20}$",   // Username: 1-20 characters, A-z, 0-9, and hyphen.
    "^[A-Za-z0-9-]{1,20}$",   // ChannelID: Same as Username.
    "^[A-Za-z0-9-]{1,128}$",  // Secret: 1-128 characters, A-z, 0-9, and hyphen.
    "^[\\x21-\\x7E]{1,20}$",  // DisplayName: 1-20 printable characters.
    "^[\\x20-\\x7E]{1,1400}$" // MessageContent: 1-1400 printable characters including space.
};

enum class ProtocolType {
    TCP,
    UDP
};

class Mail {
  public:
    int type;
    std::vector<std::string> args; // Use vector for dynamic array
};

class NetworkProtocol {
  protected:
    std::string ip;
    int port;
    int sockfd; // Socket file descriptor
    struct sockaddr_in server_addr;

  public:
    virtual void sendData(const Mail &mail) = 0;
    virtual Mail receiveData() = 0;
    virtual bool openConnection() = 0;
    virtual bool closeConnection() = 0;
    virtual ~NetworkProtocol() {}
};

//==============================================================================

class TcpProtocol : public NetworkProtocol {
  private:
    bool createSocket() {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
        return true;
    }

    bool connectToServer() {
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

  public:
    TcpProtocol(const std::string &ip, const int &port) {
        this->ip = ip;
        this->port = port;
    }
    ~TcpProtocol() {
        if (sockfd >= 0) {
            close(sockfd); // Ensure the socket is closed on destruction
        }
    }
    bool openConnection() override {
        if (!createSocket()) {
            return false;
        }
        if (!connectToServer()) {
            return false;
        }

        std::cout << "Connected successfully to " << ip << " on port " << port << std::endl;
        return true;
    }

    bool closeConnection() override {
        if (sockfd >= 0) {
            close(sockfd);
            sockfd = -1;
        }
        std::cout << "Connection closed" << std::endl;
        return true;
    }

    void sendData(const Mail &mail) override {
        int bytes_to_send = send(sockfd, mail.args[0].c_str(), mail.args[0].size(), 0);
        if (bytes_to_send < 0) {
            std::cerr << "Failed to send data" << std::endl;
        }
    }

    Mail receiveData() override {
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
};

//==============================================================================

class UdpProtocol : public NetworkProtocol {
  private:
    bool createSocket() {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
        return true;
    }

    bool connectToServer() {
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

    bool sendTo(const char *buffer, int size) {
        std::cerr << "Sending data to " << ip << " on port " << port << std::endl;
        int bytes_to_send = sendto(sockfd, buffer, size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (bytes_to_send < 0) {
            std::cerr << "Failed to send data" << std::endl;
            return false;
        }
        return true;
    }

  public:
    UdpProtocol(const std::string &ip, const int &port) {
        this->ip = ip;
        this->port = port;
    }
    ~UdpProtocol() {
        if (sockfd >= 0) {
            close(sockfd); // Ensure the socket is closed on destruction
        }
    }
    bool openConnection() override {
        if (!createSocket()) {
            return false;
        }
        if (!connectToServer()) {
            return false;
        }

        std::cout << "Connected successfully to " << ip << " on port " << port << std::endl;
        return true;
    }

    bool closeConnection() override {
        if (sockfd >= 0) {
            close(sockfd);
            sockfd = -1;
        }
        std::cout << "Connection closed" << std::endl;
        return true;
    }

    void sendData(const Mail &mail) override {
        sendTo(mail.args[0].c_str(), mail.args[0].size());
    }

    Mail receiveData() override {
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
};

class NetworkConnection {
  private:
    std::unique_ptr<NetworkProtocol> protocolPtr;

  public:
    NetworkConnection(ProtocolType type, const std::string &ip, int port) {
        if (type == ProtocolType::TCP) {
            protocolPtr = std::make_unique<TcpProtocol>(ip, port);
        } else if (type == ProtocolType::UDP) {
            protocolPtr = std::make_unique<UdpProtocol>(ip, port);
        }
    }
    void sendData(const Mail &mail) {
        if (protocolPtr) {
            protocolPtr->sendData(mail);
        }
    }
    Mail receiveData() {
        if (protocolPtr) {
            return protocolPtr->receiveData();
        } else {
            Mail mail;
            mail.type = -1;
            return mail;
        }
    }
    bool openConnection() {
        if (protocolPtr) {
            return protocolPtr->openConnection();
        } else {
            return false;
        }
    }
    bool closeConnection() {
        if (protocolPtr) {
            return protocolPtr->closeConnection();
        }
        return false;
    }
    void setProtocol(NetworkProtocol *p) {
        protocolPtr.reset(p);
    }
};

int main() {
    ProtocolType type = ProtocolType::UDP;
    NetworkConnection connection(type, "127.0.0.1", 8080);
    connection.openConnection();

    Mail mail;
    mail.type = 1;
    mail.args.push_back("Hello, World!\n");
    connection.sendData(mail);

    connection.closeConnection();

    return 0;
}
