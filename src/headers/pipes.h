#ifndef PIPES_H
#define PIPES_H

#include <fcntl.h>
#include <iostream>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

class Pipe {
    std::string name;
    int read_fd = -1;
    int write_fd = -1;

  public:
    Pipe(const std::string &name = "default")
        : name(name) {
        int fd[2];
        if (::pipe(fd) < 0) { // Use the global namespace (::) to call the POSIX pipe
            std::cerr << "Failed to create pipe\n";
        } else {
            read_fd = fd[0];
            write_fd = fd[1];
        }
    }

    ~Pipe() {
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
                std::cerr << "Failed to write to pipe: " << name << std::endl;
            }
        }
    }

    int getWriteFd() const { return write_fd; }
    int getReadFd() const { return read_fd; }
    std::string getName() const { return name; }
};

class PipeManager {
    std::map<std::string, Pipe> pipes;

  public:
    void addPipe(const std::string &name, bool readNonBlocking = false) {
        Pipe newPipe(name);
        if (readNonBlocking) {
            newPipe.makeNonBlocking();
        }
        pipes[name] = std::move(newPipe);
    }

    void writeToPipe(const std::string &name, const std::string &data) {
        auto it = pipes.find(name);
        if (it != pipes.end()) {
            it->second.write(data);
        } else {
            std::cerr << "Pipe not found: " << name << std::endl;
        }
    }
};

#endif // PIPES_H