#ifndef LISTENERS_H
#define LISTENERS_H

#include "mail-box.h"
#include "pipes.h"
#include <string>
#include <sys/epoll.h>
#include <thread>
#include <vector>

class Listener {
  private:
    std::vector<int> fds;
    std::thread listenerThread;
    MailBox *mailbox; // Use a pointer to MailBox to avoid copying and ensure thread safety
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
                    line.clear();
                }
            } else {
                line += buf[i];
            }
        }
    }

    void runListener(Listener *listener) {
        int efd = epoll_create1(0);
        if (efd == -1) {
            std::cerr << "Failed to create epoll file descriptor" << std::endl;
            return;
        }

        for (int fd : listener->fds) {
            struct epoll_event event;
            event.events = EPOLLIN; // Read operation | Edge Triggered
            event.data.fd = fd;
            if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event) == -1) {
                std::cerr << "Failed to add file descriptor to epoll" << std::endl;
                close(efd);
                return;
            }
        }

        struct epoll_event events[10]; // Buffer where events are returned
        // Event loop
        while (true) {
            int n = epoll_wait(efd, events, 10, -1);
            for (int i = 0; i < n; i++) {
                if (events[i].events & EPOLLIN) {
                    listenStdin(events[i].data.fd, efd);
                }
            }
        }

        close(efd); // Ideally, this should be in a cleanup section or use RAII to manage resources
    }

  public:
    Listener(const std::vector<int> &fds)
        : fds(fds) {
        // Launch the listening thread, safely passing 'this'
        listenerThread = std::thread([this] { this->runListener(this); });
    }

    ~Listener() {
        // Ensure the listener thread completes before destroying the object.
        if (listenerThread.joinable()) {
            listenerThread.join();
        }

        // Close file descriptors.
        for (const auto &fd : fds) {
            close(fd);
        }

        // print in red
        std::cout << "\033[1;31mListener destroyed\033[0m" << std::endl;
    }
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