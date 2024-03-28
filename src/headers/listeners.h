#ifndef LISTENERS_H
#define LISTENERS_H

#include "mail-box.h"
#include "net.h"
#include "pipes.h"
#include "stopwatch.h"
#include <bits/algorithmfwd.h>
#include <string>
#include <sys/epoll.h>
#include <thread>
#include <unordered_map>
#include <vector>

class Listener {
  private:
    std::unordered_map<int, fdType> fdMap;
    std::thread listenerThread;
    MailBox *mailbox;
    Mail mail;
    NetworkConnection *connection;
    ProtocolType protocolType;
    bool receivedConfirm = true; // will send new packets only after receiving confirm
    bool toSendRegistered = true;
    int retries = 0;
    int refMsgId = -1; // To check if CONFIRM ID matches the sent message ID
    int refAuthId = -1;
    std::atomic<bool> keepRunning = true;
    bool sentBye = false;

    bool listenStdin(int fd, int efd) {

        char buf[1024];
        ssize_t count = read(fd, buf, sizeof(buf) - 1);

        if (count == -1) {
            if (errno != EAGAIN) {
                std::cerr << "Read error" << std::endl;
                close(efd);
                return false;
            }
        } else if (count == 0) {
            std::cout << "Pipe closed" << std::endl;
            close(efd);
            return false;
        } else {
            buf[count] = '\0';
        }

        // Create a Mail for each line read from the pipe
        // get line from buffer
        std::string line;
        for (int i = 0; i < count; i++) {
            if (buf[i] == '\n') {
                if (!line.empty()) {

                    if (!mailbox->writeMail(line, mail)) {
                        std::cerr << "Failed to write mail" << std::endl;
                        close(efd);
                        return false;
                    }

                    mailbox->addMail(mail);
                    line.clear();
                }
            } else {
                line += buf[i];
            }
        }

        return true;
    }

    void registerFds(const std::unordered_map<int, fdType> &fdMap, int efd) {
        for (const auto &fd_pair : fdMap) {
            int fd = fd_pair.first;
            struct epoll_event event;
            event.events = EPOLLIN; // Read operation | Edge Triggered
            event.data.fd = fd;
            if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event) == -1) {
                std::cerr << "Failed to add file descriptor to epoll, error: " << strerror(errno) << std::endl;
                std::cerr << "File descriptor: " << std::to_string(fd) << " with name: " << std::to_string(fd_pair.second) << " not added to epoll" << std::endl;
                close(efd);
                return;
            }
            printGreen("File descriptor: " + std::to_string(fd) + " with name: " + std::to_string(fd_pair.second) + " added to epoll");
        }
    }

    void unregisterFd(int fd, int efd) {
        if (epoll_ctl(efd, EPOLL_CTL_DEL, fd, nullptr) == -1) {
            std::cerr << "Failed to remove file descriptor from epoll" << std::endl;
            // You might choose to handle the error differently depending on your application's requirements
        } else {
            printGreen("File descriptor: " + std::to_string(fd) + " removed from epoll");
        }
    }

    void registerFd(int fd, int efd) {
        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = fd;
        if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event) == -1) {
            std::cerr << "Failed to add file descriptor to epoll, error: " << strerror(errno) << std::endl;
            close(efd);
            return;
        }
        printGreen("File descriptor: " + std::to_string(fd) + " added to epoll");
    }

    bool listenSocket(char *buffer, int efd) {
        if (!connection->receiveData(buffer)) {
            std::cerr << "Failed to receive data" << std::endl;
            close(efd);
            return false;
        }

        if (!mailbox->writeMail(&buffer, mail)) {
            std::cerr << "Failed to write mail" << std::endl;
            close(efd);
            return false;
        }

        // For UDP
        if (protocolType == ProtocolType::UDP) {

            Mail confirmMail;
            mailbox->writeMail(Mail::MessageType::CONFIRM, confirmMail, mail.getMessageID());

            switch (mail.type) {
            case Mail::MessageType::REPLY:
                if (mail.getMessageID() <= mailbox->srvMsgId) { // Message has already been received
                    printRed("Message has already been received");
                    connection->sendData(confirmMail);
                    std::cout << "Sent confirm" << std::endl;
                    return true;
                }
                if (refAuthId != mail.getRefMessageID()) {
                    printRed("Invalid refAuthId");
                    return true; // TODO: shouldt we terminate the connection here?
                }
                mailbox->srvMsgId = mail.getMessageID();
                printGreen("srvMsgId updated to: " + std::to_string(mailbox->srvMsgId));
                break;
            case Mail::MessageType::CONFIRM:
                if (std::get<Mail::ConfirmMessage>(mail.data).RefMessageID == refMsgId && receivedConfirm == false) {
                    receivedConfirm = true;
                    retries = 0;
                    refMsgId = -1; // No longer waiting for CONFIRM
                    printGreen("Received CONFIRM");
                }
                mail.addToMailQueue = false;
                return true;
                break;
            default:
                if (mail.getMessageID() <= mailbox->srvMsgId) { // Message has already been received
                    printRed("Message has already been received");
                    return true;
                }
                mailbox->srvMsgId = mail.getMessageID();
                break;
            }

            connection->sendData(confirmMail);
            std::cout << "Sent confirm" << std::endl;
        }

        mailbox->addMail(mail);

        return true;
    }

    bool listenToSendPipe(int fd, int efd) {
        char buf[1];
        ssize_t count = read(fd, buf, sizeof(buf)); // Clear the pipe
        if (count == -1) {
            if (errno != EAGAIN) {
                std::cerr << "Read error" << std::endl;
                close(efd);
                return false;
            }
        } else if (count == 0) {
            std::cout << "Pipe closed" << std::endl;
            close(efd);
            return false;
        }
        mail = mailbox->getOutgoingMail();
        mail.printMail();

        connection->sendData(mail);

        // For UDP
        if (protocolType == ProtocolType::UDP) {
            receivedConfirm = false;
            refMsgId = mail.getMessageID();

            if (mail.type == Mail::MessageType::AUTH) {
                refAuthId = mail.getMessageID();
                printGreen("refAuthId updated to: " + std::to_string(refAuthId));
            }
        }

        if (mail.type == Mail::MessageType::BYE) {
            sentBye = true;
        }

        return true;
    }

    void runListener() {
        int efd = epoll_create1(0);
        if (efd == -1) {
            std::cerr << "Failed to create epoll file descriptor" << std::endl;
            return;
        }

        // Register file descriptors with epoll
        registerFds(fdMap, efd);

        int timeout = -1; // Wait indefinitely

        StopWatch stopWatch;

        struct epoll_event events[10]; // Buffer where events are returned
        while (keepRunning.load() || !sentBye || !receivedConfirm) {
            // TODO: Remove this comment: LMAOOO THIS IS UGLY AS FUCK
            if (!receivedConfirm && toSendRegistered) {
                unregisterFd(mailbox->getNotifyListenerPipe()->getReadFd(), efd);
                toSendRegistered = false;
            } else if (receivedConfirm && !toSendRegistered) {
                registerFd(mailbox->getNotifyListenerPipe()->getReadFd(), efd);
                toSendRegistered = true;
            }

            if (!receivedConfirm) {
                timeout = connection->getTimeout() - stopWatch.duration();
            } else {
                timeout = -1;
            }

            std::cout << "setting timer\n";
            stopWatch.start();
            std::cout << "waiting for fds for: " << timeout << " mili-seconds\n";
            int n = epoll_wait(efd, events, 10, timeout);
            stopWatch.stop();

            if (n == 0 && retries >= connection->getRetries()) { // Time has ran out
                std::cerr << "Error: Server not responding" << std::endl;
                return;
            } else if (n == 0) { // You have few more tries
                stopWatch.reset();
                retries++;
                printRed("Awaiting confirmation failed. Retrying... (retries left: " + std::to_string(connection->getRetries() - retries + 1) + ")");
            }

            for (int i = 0; i < n; i++) {
                std::cout << "Event on FD: " << events[i].data.fd << std::endl;

                int activeFd = events[i].data.fd;

                // Look up the type of the active file descriptor
                // Must be EPOLLIN
                auto it = fdMap.find(activeFd);
                if (it != fdMap.end() && EPOLLIN) {
                    fdType type = it->second;

                    char buffer[1024] = {0};

                    switch (type) {
                    case StdinPipe:
                        printBlue("StdinPipe");
                        if (!listenStdin(activeFd, efd)) {
                            printRed("Failed to listen to stdin\n");
                            return;
                        }
                        break;
                    case SocketPipe:
                        printBlue("SocketPipe");
                        if (!listenSocket(buffer, efd)) {
                            printRed("Failed to listen to socket\n");
                            return;
                        }
                        break;
                    case ToSendPipe: {
                        printBlue("ToSendPipe");
                        if (!listenToSendPipe(activeFd, efd)) {
                            printRed("Failed to listen to send pipe\n");
                            return;
                        }
                        break;
                    }
                    case Unknown:
                        std::cerr << "Unknown file descriptor type" << std::endl;
                        break;
                        // ... other cases ...
                    }
                }
            }

            // Send or expect CONFIRM based on Flags
            printBlue("keepRunning: " + std::to_string(keepRunning.load()) + ", receivedConfirm: " + std::to_string(receivedConfirm) + ", sentBye: " + std::to_string(sentBye));
        }
        close(efd);
    }

  public:
    Listener(MailBox *mailbox, NetworkConnection *connection, ProtocolType protocolType)
        : mailbox(mailbox), connection(connection), protocolType(protocolType) {
    }

    ~Listener() {

        // Ensure the listener thread completes before destroying the object
        if (listenerThread.joinable()) {
            listenerThread.join();
        }

        // Close all file descriptors
        for (const auto &fd_pair : fdMap) {
            close(fd_pair.first); // fd_pair.first is the file descriptor
        }

        // Print a message indicating the Listener is destroyed
        printRed("Listener destroyed");
    }

    void run() {
        listenerThread = std::thread([this] { this->runListener(); });
    }

    void stop() {
        printRed("Stopping listener");
        // Set a flag to signal the listener thread to stop
        keepRunning.store(false);

        // If the thread is running, wait for it to finish
        if (listenerThread.joinable()) {
            listenerThread.join();
        }

        // Close all file descriptors to ensure proper resource deallocation
        for (const auto &fd_pair : fdMap) {
            close(fd_pair.first);
        }

        // Print a message indicating the Listener has stopped
        printRed("Listener stopped");
    }

    void addFd(int fd, fdType type) {
        fdMap[fd] = type;
    }
};

class StdinListener {
  private:
    std::thread listenerThread;
    Pipe *pipe;
    std::atomic<bool> keepRunning;

    static void runListener(StdinListener *listener) {
        std::string line;
        while (listener->keepRunning.load()) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(STDIN_FILENO, &read_fds);

            // Set timeout (e.g., 1 second)
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            // Check if stdin has data
            int select_result = select(STDIN_FILENO + 1, &read_fds, nullptr, nullptr, &timeout);

            if (select_result == -1) {
                perror("select"); // Error occurred in select
                break;
            } else if (select_result > 0) {
                // Data is available to read
                if (FD_ISSET(STDIN_FILENO, &read_fds)) {
                    if (std::getline(std::cin, line)) {
                        listener->pipe->write(line + "\n");
                    } else {
                        // Break if getline fails (e.g., EOF or error)
                        break;
                    }
                }
            }
            // If select_result is 0, the timeout occurred, and the loop continues
        }
    }

  public:
    StdinListener(Pipe *pipe)
        : pipe(pipe), keepRunning(true) {
        listenerThread = std::thread(runListener, this);
    }

    ~StdinListener() {
        if (listenerThread.joinable()) {
            keepRunning.store(false);
            listenerThread.join();
        }
        std::cout << "\033[1;31mStdInListener destroyed\033[0m" << std::endl;
    }

    void stop() {
        keepRunning.store(false);
        if (listenerThread.joinable()) {
            listenerThread.join();
        }
    }

    // Delete copy constructor and copy assignment operator to prevent copying
    StdinListener(const StdinListener &) = delete;
    StdinListener &operator=(const StdinListener &) = delete;
};
#endif // LISTENERS_H