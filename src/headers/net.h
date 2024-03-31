#ifndef NET_H
#define NET_H

#include <memory> // For std::unique_ptr
#include <string>
#include <unistd.h> // Depending on your needs, this might be used for sleep, etc.

#include <arpa/inet.h> // For inet_pton
#include <cstring>     // For memset
#include <iostream>
#include <netinet/in.h> // For sockaddr_in
#include <sys/epoll.h>
#include <sys/socket.h> // For socket functions

#include "mail-box.h"
#include "net-utils.h"
#include "stopwatch.h"

#include <arpa/inet.h>  // For inet_pton()
#include <cstring>      // For memset
#include <iostream>     // For std::cerr
#include <netdb.h>      // For getaddrinfo, freeaddrinfo, addrinfo
#include <netinet/in.h> // For sockaddr_in, htons()
#include <sys/socket.h> // For socket(), connect()
#include <sys/types.h>  // For types used in sys/socket.h
#include <unistd.h>     // For close()

#include <iomanip>

//=================================NetworkProtocol=============================================
/**
 * @brief Abstract class for network protocols.
 *
 * This class is an abstract class for network protocols. It provides a common interface for
 * different network protocols such as TCP and UDP. The class has pure virtual functions for
 * sending and receiving data, opening and closing connections.
 *
 * @param ip IP address of the server.
 * @param port Port number of the server.
 * @param sockfd Socket file descriptor.
 * @param server_addr Server address.
 *
 * @return None.
 *
 * @note None.
 *
 * @see TcpProtocol, UdpProtocol
 */
class NetworkProtocol {
  protected:
    std::string ip;
    uint16_t port;
    int sockfd; // Socket file descriptor
    struct sockaddr_in server_addr;

    // only for UDP
    int timeout = -1;
    int retries = -1;

  public:
    virtual void sendData(const Mail &mail) = 0;
    virtual bool receiveData(char *buffer) = 0;
    virtual bool openConnection() = 0;
    virtual bool closeConnection() = 0;
    virtual ~NetworkProtocol(){};

    int getTimeout() { return timeout; }
    int getRetries() { return retries; }

    int getFdsocket() { return sockfd; }
};

//=================================TcpProtocol=============================================

class TcpProtocol : public NetworkProtocol {
  private:
    bool createSocket();
    bool connectToServer();

  public:
    TcpProtocol(const std::string &ip, const uint16_t &port);
    ~TcpProtocol();
    bool openConnection() override;
    bool closeConnection() override;
    void sendData(const Mail &mail) override;
    bool receiveData(char *buffer) override;
};

//==================================UdpProtocol============================================

class UdpProtocol : public NetworkProtocol {
  private:
    bool createSocket();
    bool connectToServer();
    bool sendTo(const char *buffer, int size);
    bool getConfirm();
    void sendConfirm(uint16_t seq);
    std::string convertBufferToHexString(const std::vector<char> &buffer);

  public:
    UdpProtocol(const std::string &ip, const uint16_t &port, const uint16_t &timeout = 0, const uint16_t &retries = 0);
    ~UdpProtocol();
    bool openConnection() override;
    bool closeConnection() override;
    void sendData(const Mail &mail) override;
    bool receiveData(char *buffer) override;
};
// TODO: implemnt dynamic port allocation
//==================================NetworkConnection============================================

class NetworkConnection {
  private:
    std::unique_ptr<NetworkProtocol> protocolPtr;

  public:
    NetworkConnection(ProtocolType type, const std::string &ip, const uint16_t &port, const uint16_t &timeout = 0, const uint16_t &retries = 0);
    void sendData(const Mail &mail);
    bool receiveData(char *buffer);
    bool openConnection();
    bool closeConnection();
    void setProtocol(NetworkProtocol *p);
    int getFdsocket();
    int getTimeout() { return protocolPtr->getTimeout(); }
    int getRetries() { return protocolPtr->getRetries(); }
};

#endif // NET_H