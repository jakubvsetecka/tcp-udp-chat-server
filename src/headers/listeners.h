#ifndef LISTENERS_H
#define LISTENERS_H

#include "mail-box.h"
#include "pipes.h"
#include <bits/algorithmfwd.h>
#include <string>
#include <sys/epoll.h>
#include <thread>
#include <unordered_map>
#include <vector>

enum fdType {
    StdinPipe,
    Unknown
};

class Listener {
  private:
    std::unordered_map<int, fdType> fdMap;
    std::thread listenerThread;
    // Assuming MailBox and Mail are defined elsewhere
    MailBox *mailbox;
    Mail mail;

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
                    mail = mailbox->writeMail(line);
                    mail.printMail();
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
                int activeFd = events[i].data.fd;

                // Look up the type of the active file descriptor
                auto it = fdMap.find(activeFd);
                if (it != fdMap.end()) {
                    fdType type = it->second;

                    switch (type) {
                    case StdinPipe:
                        listenStdin(activeFd, efd);
                        break;
                        // ... other cases ...
                    }
                }
            }
        }
        close(efd);
    }

  public:
    void start() {
        listenerThread = std::thread([this] { this->runListener(); });
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

    void addFd(int fd, fdType type) {
        fdMap[fd] = type;
    }

    // Additional getters or utility methods as needed...
};

class StdinListener {
  private:
    std::thread listenerThread;
    Pipe *pipe; // Use a pointer to Pipe to avoid copying and ensure thread safety

    static void runListener(StdinListener *listener) {
        std::string line;
        while (std::getline(std::cin, line)) {  // Read a line from stdin
            listener->pipe->write(line + "\n"); // Write it to the pipe directly
        }
    }

  public:
    StdinListener(Pipe *pipe)
        : pipe(pipe) {
        // Launch the listening thread, safely passing 'this'
        listenerThread = std::thread(runListener, this);
    }

    ~StdinListener() {
        // Ensure the listener thread completes before destroying the object.
        if (listenerThread.joinable()) {
            listenerThread.join();
        }
        std::cout << "\033[1;31mStdInListener destroyed\033[0m" << std::endl;
    }

    // Delete copy constructor and copy assignment operator to prevent copying
    StdinListener(const StdinListener &) = delete;
    StdinListener &operator=(const StdinListener &) = delete;
};
#endif // LISTENERS_H