#include "listeners.h"
#include <errno.h>
#include <iostream>
#include <stdexcept>
#include <sys/epoll.h>
#include <unistd.h>

// Listener class method implementations
Listener::Listener(MailBox *mailbox, NetworkConnection *connection, ProtocolType protocolType)
    : mailbox(mailbox), connection(connection), protocolType(protocolType) {}

Listener::~Listener() {

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

void Listener::run() {
    listenerThread = std::thread([this] { this->runListener(); });
}

void Listener::stop() {
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

void Listener::addFd(int fd, fdType type) {
    fdMap[fd] = type;
}

void Listener::handleSignal(int signum) {
    if (write(writeSignalFd, "1", 1) == -1) {
        throw std::runtime_error("Failed to write to signal pipe");
    }
}

bool Listener::listenStdin(int fd, int efd) {

    char buf[1024];
    ssize_t count = read(fd, buf, sizeof(buf) - 1);

    if (count == -1) {
        if (errno != EAGAIN) {
            throw std::runtime_error("Failed to read from stdin");
            close(efd);
            return false;
        }
    } else if (count == 0) {
        printRed("Pipe closed");
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
                Mail mail;
                if (!mailbox->writeMail(line, mail)) {
                    throw std::runtime_error("Failed to write mail from stdin");
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

void Listener::registerFds(const std::unordered_map<int, fdType> &fdMap, int efd) {
    for (const auto &fd_pair : fdMap) {
        int fd = fd_pair.first;
        struct epoll_event event;
        event.events = EPOLLIN; // Read operation | Edge Triggered
        event.data.fd = fd;
        if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event) == -1) {
            throw std::runtime_error("Failed to add file descriptor to epoll");
            close(efd);
            return;
        }
        printGreen("File descriptor: " + std::to_string(fd) + " with name: " + std::to_string(fd_pair.second) + " added to epoll");
    }
}

void Listener::unregisterFd(int fd, int efd) {
    if (epoll_ctl(efd, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        throw std::runtime_error("Failed to remove file descriptor from epoll");
    } else {
        printGreen("File descriptor: " + std::to_string(fd) + " removed from epoll");
    }
}

void Listener::registerFd(int fd, int efd) {
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = fd;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event) == -1) {
        throw std::runtime_error("Failed to add file descriptor to epoll");
        close(efd);
        return;
    }
    printGreen("File descriptor: " + std::to_string(fd) + " added to epoll");
}

bool Listener::listenSocket(char *buffer, int efd) {
    if (!connection->receiveData(buffer)) {
        throw std::runtime_error("Failed to receive data");
        close(efd);
        return false;
    }

    Mail mail;
    if (!mailbox->writeMail(&buffer, mail)) {
        std::cerr << "Failed to write mail from socket" << std::endl;
        keepRunning.store(false);
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
                printGreen("Sent confirm for message: " + std::to_string(mail.getMessageID()) + " with srvMsgId: " + std::to_string(mailbox->srvMsgId) + " and refAuthId: " + std::to_string(refAuthId) + " and refMsgId: " + std::to_string(refMsgId));
                return true;
            }
            if (refAuthId != mail.getRefMessageID()) {
                printRed("Invalid refAuthId");
                printRed("refAuthId: " + std::to_string(refAuthId) + ", refMessageID: " + std::to_string(mail.getRefMessageID()));
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
        printGreen("Sent confirm for message: " + std::to_string(mail.getMessageID()) + " with srvMsgId: " + std::to_string(mailbox->srvMsgId) + " and refAuthId: " + std::to_string(refAuthId) + " and refMsgId: " + std::to_string(refMsgId));
    }

    mailbox->addMail(mail);

    return true;
}

bool Listener::listenToSendPipe(int fd, int efd) {
    char buf[1];
    ssize_t count = read(fd, buf, sizeof(buf)); // Clear the pipe
    if (count == -1) {
        if (errno != EAGAIN) {
            throw std::runtime_error("Failed to read from send pipe");
            close(efd);
            return false;
        }
    } else if (count == 0) {
        printRed("Pipe closed");
        close(efd);
        return false;
    }
    Mail mail = mailbox->getOutgoingMail();
    if (verbose) {
        printGreen("Sending mail: ");
        mail.printMail();
    }

    if (mail.type == Mail::MessageType::BYE) {
        sentBye = true;
        keepRunning.store(false);
    }

    connection->sendData(mail);

    // For UDP
    if (protocolType == ProtocolType::UDP) {
        receivedConfirm = false;
        refMsgId = mail.getMessageID();
        confMail = mail;

        if (mail.type == Mail::MessageType::AUTH || mail.type == Mail::MessageType::JOIN) {
            refAuthId = mail.getMessageID();
            printGreen("refAuthId updated to: " + std::to_string(refAuthId));
        }
    }

    return true;
}

bool Listener::listenSignal(int fd, int efd) {
    char buf[1];
    ssize_t count = read(fd, buf, sizeof(buf)); // Clear the pipe
    if (count == -1) {
        if (errno != EAGAIN) {
            throw std::runtime_error("Failed to read from signal pipe");
            close(efd);
            return false;
        }
    } else if (count == 0) {
        printRed("Pipe closed");
        close(efd);
        return false;
    }

    Mail mail;
    mailbox->writeMail(Mail::MessageType::ERR, mail);
    mail.sigint = true;
    mailbox->addMail(mail);

    keepRunning.store(false);

    return true;
}

void Listener::runListener() {
    try {
        int efd = epoll_create1(0);
        if (efd == -1) {
            throw std::runtime_error("Failed to create epoll file descriptor");
            return;
        }

        // Register file descriptors with epoll
        registerFds(fdMap, efd);

        int timeout = -1; // Wait indefinitely

        StopWatch stopWatch;

        struct epoll_event events[10]; // Buffer where events are returned
        while (keepRunning.load() || (!sentBye || !receivedConfirm)) {
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

            printWhite("setting timer");
            stopWatch.start();
            printWhite("waiting for fds for: " + std::to_string(timeout) + " mili-seconds");
            int n = epoll_wait(efd, events, 10, timeout);
            stopWatch.stop();

            if (n == 0 && retries >= connection->getRetries()) { // Time has ran out
                throw std::runtime_error("Server not responding");
            } else if (n == 0) { // You have few more tries
                stopWatch.reset();
                retries++;
                printWhite("Awaiting confirmation failed. Retrying... (retries left: " + std::to_string(connection->getRetries() - retries + 1) + ")");

                connection->sendData(confMail);
            }

            for (int i = 0; i < n; i++) {
                printWhite("Event on FD: " + std::to_string(events[i].data.fd));

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
                            throw std::runtime_error("Failed to listen to stdin");
                        }
                        break;
                    case SocketPipe:
                        printBlue("SocketPipe");
                        if (!listenSocket(buffer, efd)) {
                            throw std::runtime_error("Failed to listen to socket");
                        }
                        break;
                    case ToSendPipe: {
                        printBlue("ToSendPipe");
                        if (!listenToSendPipe(activeFd, efd)) {
                            throw std::runtime_error("Failed to listen to send pipe");
                        }
                        break;
                    }
                    case SignalPipe:
                        printBlue("SignalPipe");
                        if (!listenSignal(activeFd, efd)) {
                            throw std::runtime_error("Failed to listen to signal pipe");
                        }
                        break;
                    case Unknown:
                        throw std::runtime_error("Unknown file descriptor type");
                        break;
                        // ... other cases ...
                    }
                }
            }

            // Send or expect CONFIRM based on Flags
            printBlue("keepRunning: " + std::to_string(keepRunning.load()) + ", receivedConfirm: " + std::to_string(receivedConfirm) + ", sentBye: " + std::to_string(sentBye));
        }
        close(efd);
    } catch (const std::runtime_error &e) {
        std::cerr << "ERR: " << e.what() << std::endl;
        exit(1);
    }
}
// StdinListener class method implementations

StdinListener::StdinListener(Pipe *pipe)
    : pipe(pipe), keepRunning(true) {
    listenerThread = std::thread([this] { runListener(this); });
}

StdinListener::~StdinListener() {
    if (listenerThread.joinable()) {
        keepRunning.store(false);
        listenerThread.join();
    }
}

void StdinListener::stop() {
    keepRunning.store(false);
    if (listenerThread.joinable()) {
        listenerThread.join();
    }
}

void StdinListener::runListener(StdinListener *listener) {
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
                    printRed("Failed to read from stdin");
                    if (write(writeSignalFd, "1", 1) == -1) {
                        throw std::runtime_error("Failed to write to signal pipe");
                    }
                    break;
                }
            }
        }
        // If select_result is 0, the timeout occurred, and the loop continues
    }
}
