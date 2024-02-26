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

  public:
    virtual void sendData(const Mail &mail) = 0;
    virtual Mail receiveData() = 0;
    virtual bool openConnection() = 0;
    virtual void closeConnection() = 0;
    virtual ~NetworkProtocol() {}
};

class TcpProtocol : public NetworkProtocol {
  private:
    void sendData(const Mail &mail) override {}
    Mail receiveData() override {}
    void closeConnection() override {}

    bool createSocket() {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
        return true;
    }

    bool connectToServer() {
        struct sockaddr_in server_addr;
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
};

class UdpProtocol : public NetworkProtocol {
  private:
    void sendData(const Mail &mail) override;
    Mail receiveData() override;
    bool openConnection() override;
    void closeConnection() override;

  public:
    UdpProtocol(const std::string &ip, const int &port) {
        this->ip = ip;
        this->port = port;
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
    void closeConnection() {
        if (protocolPtr) {
            return protocolPtr->closeConnection();
        }
    }
    void setProtocol(NetworkProtocol *p) {
        protocolPtr.reset(p);
    }
};

int main() {
    ProtocolType type = ProtocolType::TCP;
    NetworkConnection connection(type, "127.0.0.1", 8080);
    connection.openConnection();

    return 0;
}
