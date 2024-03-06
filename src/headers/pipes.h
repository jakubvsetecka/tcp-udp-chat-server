#ifndef PIPES_H
#define PIPES_H

#include "utils.h"
#include <fcntl.h>
#include <iostream>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

enum fdType {
    StdinPipe,
    SocketPipe,
    ToSendPipe,
    Unknown
};

class Pipe {
    fdType type;
    int read_fd = -1;
    int write_fd = -1;

  public:
    Pipe(const fdType &type = fdType::Unknown)
        : type(type) {
        int fd[2];
        if (::pipe(fd) < 0) { // Use the global namespace (::) to call the POSIX pipe
            std::cerr << "Failed to create pipe\n";
        } else {
            read_fd = fd[0];
            write_fd = fd[1];
        }
    }

    ~Pipe() {
        printRed("Pipe destructor called on: " + std::to_string(type) + "\n");
        if (read_fd != -1) close(read_fd);
        if (write_fd != -1) close(write_fd);
    }

    void makeNonBlocking() {
        if (read_fd != -1) {
            int flags = fcntl(read_fd, F_GETFL, 0);
            if (flags != -1) {
                fcntl(read_fd, F_SETFL, flags | O_NONBLOCK);
            }
        }
    }

    void write(const std::string &data) {
        if (write_fd != -1) {
            ssize_t bytes_written = ::write(write_fd, data.c_str(), data.size());
            if (bytes_written == -1) {
                std::cerr << "Failed to write to pipe: " << type << std::endl;
            }
        }
    }

    int getWriteFd() const { return write_fd; }
    int getReadFd() const {
        return read_fd;
    }
    fdType getType() const { return type; }
};

class PipeManager {
    std::map<fdType, Pipe> pipes;

  public:
    void addPipe(fdType type, bool readNonBlocking = false) {
        Pipe newPipe; // Assuming Pipe constructor can be default or modified
        if (readNonBlocking) {
            newPipe.makeNonBlocking();
        }
        pipes[type] = std::move(newPipe);
    }

    void writeToPipe(const fdType &type, const std::string &data) {
        auto it = pipes.find(type);
        if (it != pipes.end()) {
            it->second.write(data);
        } else {
            std::cerr << "Pipe not found: " << type << std::endl;
        }
    }
};

#endif // PIPES_H