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

    void listenStdin(int fd, int efd) {

        char buf[1024];
        ssize_t count = read(fd, buf, sizeof(buf) - 1);

        if (count == -1) {
            if (errno != EAGAIN) {
                std::cerr << "Read error" << std::endl;
                close(efd);
                return;
            }
        } else if (count == 0) {
            std::cout << "Pipe closed" << std::endl;
            close(efd);
            return;
        } else {
            buf[count] = '\0';
            std::cout << "Read from pipe: " << buf << std::endl;
        }

        // Create a Mail for each line read from the pipe
        // get line from buffer
        std::string line;
        for (int i = 0; i < count; i++) {
            if (buf[i] == '\n') {
                if (!line.empty()) {
                    std::cout << "Line: " << line << std::endl;
                    mailbox->writeMail(line, mail);
                    mailbox->addMail(mail);
                    printGreen("Mail added to mailbox");
                    line.clear();
                }
            } else {
                line += buf[i];
            }
        }
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
                        listenStdin(activeFd, efd);
                        break;
                    case SocketPipe:
                        std::cout << "SocketPipe" << std::endl;
                        if (!connection->receiveData(buffer)) {
                            std::cerr << "Failed to receive data" << std::endl;
                            close(efd);
                            return;
                        }
                        std::cout << "Received mail" << std::endl;
                        mailbox->writeMail(buffer, mail); // Pass buffer as a string argument
                        mailbox->addMail(mail);
                        std::cout << "Mail added to mailbox" << std::endl;
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
        std::cout << "\033[1;31mListener destroyed\033[0m" << std::endl;
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