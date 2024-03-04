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
    uint16_t timeout = 0;
    uint16_t retries = 0;

  public:
    virtual void sendData(const Mail &mail) = 0;
    virtual Mail receiveData() = 0;
    virtual bool openConnection() = 0;
    virtual bool closeConnection() = 0;
    virtual ~NetworkProtocol() {}
};

//=================================TcpProtocol=============================================

class TcpProtocol : public NetworkProtocol {
  private:
    bool createSocket();
    bool connectToServer();

  public:
    TcpProtocol(const std::string &ip, const uint16_t &port, const uint16_t &timeout = 0, const uint16_t &retries = 0);
    ~TcpProtocol();
    bool openConnection() override;
    bool closeConnection() override;
    void sendData(const Mail &mail) override;
    Mail receiveData() override;
};

//==================================UdpProtocol============================================

class UdpProtocol : public NetworkProtocol {
  private:
    bool createSocket();
    bool connectToServer();
    bool sendTo(const char *buffer, int size);
    bool getConfirm();
    void sendConfirm(uint16_t seq);

    uint16_t messageID = 0;

  public:
    UdpProtocol(const std::string &ip, const uint16_t &port, const uint16_t &timeout = 0, const uint16_t &retries = 0);
    ~UdpProtocol();
    bool openConnection() override;
    bool closeConnection() override;
    void sendData(const Mail &mail) override;
    Mail receiveData() override;
};
// TODO: implemnt dynamic port allocation
//==================================NetworkConnection============================================

class NetworkConnection {
  private:
    std::unique_ptr<NetworkProtocol> protocolPtr;

  public:
    NetworkConnection(ProtocolType type, const std::string &ip, const uint16_t &port, const uint16_t &timeout = 0, const uint16_t &retries = 0);
    void sendData(const Mail &mail);
    Mail receiveData();
    bool openConnection();
    bool closeConnection();
    void setProtocol(NetworkProtocol *p);
};

#endif // NET_H