#ifndef LISTENERS_H
#define LISTENERS_H

#include "mail-box.h"
#include "net.h"
#include "pipes.h"
#include "stopwatch.h"
#include <string>
#include <sys/epoll.h>
#include <thread>
#include <unordered_map>
#include <vector>

extern int writeSignalFd;

class Listener {
  private:
    std::unordered_map<int, fdType> fdMap;
    std::thread listenerThread;
    MailBox *mailbox;
    Mail confMail;
    NetworkConnection *connection;
    ProtocolType protocolType;
    bool receivedConfirm = true;
    bool toSendRegistered = true;
    int retries = 0;
    int refMsgId = -1;
    int refAuthId = -1;
    std::atomic<bool> keepRunning = true;
    bool sentBye = false;

    bool listenStdin(int fd, int efd);
    void registerFds(const std::unordered_map<int, fdType> &fdMap, int efd);
    void unregisterFd(int fd, int efd);
    void registerFd(int fd, int efd);
    bool listenSocket(char *buffer, int efd);
    bool listenToSendPipe(int fd, int efd);
    bool listenSignal(int fd, int efd);
    void runListener();

  public:
    Listener(MailBox *mailbox, NetworkConnection *connection, ProtocolType protocolType);
    ~Listener();
    void run();
    void stop();
    void addFd(int fd, fdType type);
    static void handleSignal(int signum);
};

class StdinListener {
  private:
    std::thread listenerThread;
    Pipe *pipe;
    std::atomic<bool> keepRunning;

    static void runListener(StdinListener *listener);

  public:
    StdinListener(Pipe *pipe);
    ~StdinListener();
    void stop();
    StdinListener(const StdinListener &) = delete;
    StdinListener &operator=(const StdinListener &) = delete;
};

#endif // LISTENERS_H
