#ifndef LISTENERS_H
#define LISTENERS_H

#include "mail-box.h"
#include "net.h"
#include "pipes.h"
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
                std::cerr << "Failed to add file descriptor to epoll" << std::endl;
                close(efd);
                return;
            }
            printGreen("File descriptor: " + std::to_string(fd) + " with name: " + std::to_string(fd_pair.second) + " added to epoll");
        }
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

        mailbox->addMail(mail);

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

        struct epoll_event events[10]; // Buffer where events are returned
        while (true) {
            int n = epoll_wait(efd, events, 10, -1);
            for (int i = 0; i < n; i++) {
                std::cout << "Event on FD: " << events[i].data.fd << std::endl;

                int activeFd = events[i].data.fd;

                // Look up the type of the active file descriptor
                auto it = fdMap.find(activeFd);
                if (it != fdMap.end()) {
                    fdType type = it->second;

                    char buffer[1024] = {0};

                    switch (type) {
                    case StdinPipe:
                        printBlue("StdinPipe");
                        if (!listenStdin(activeFd, efd)) {
                            printRed("Failed to listen to stdin\n");
                            return;
                        }
                        printGreen("Mail added to mailbox\n");
                        break;
                    case SocketPipe:
                        printBlue("SocketPipe");
                        if (!listenSocket(buffer, efd)) {
                            printRed("Failed to listen to socket\n");
                            return;
                        }
                        printGreen("Mail added to mailbox\n");
                        break;
                    case ToSendPipe:
                        // ... other cases ...
                        break;
                    case Unknown:
                        std::cerr << "Unknown file descriptor type" << std::endl;
                        break;
                        // ... other cases ...
                    }
                }
            }

            // Send or expect CONFIRM based on Flags
        }
        close(efd);
    }

  public:
    Listener(MailBox *mailbox, NetworkConnection *connection)
        : mailbox(mailbox), connection(connection) {
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

    void addFd(int fd, fdType type) {
        fdMap[fd] = type;
    }

    // Additional getters or utility methods as needed...
};

class StdinListener {
  private:
    std::thread listenerThread;
    Pipe *pipe;
    std::atomic<bool> keepRunning;

    static void runListener(StdinListener *listener) {
        std::string line;
        while (listener->keepRunning.load()) {
            if (std::getline(std::cin, line)) {
                listener->pipe->write(line + "\n");
            } else {
                break; // Exit if std::cin is closed or in a bad state
            }
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

    // Delete copy constructor and copy assignment operator to prevent copying
    StdinListener(const StdinListener &) = delete;
    StdinListener &operator=(const StdinListener &) = delete;
};
#endif // LISTENERS_H