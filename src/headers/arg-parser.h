#ifndef ARG_PARSER_H
#define ARG_PARSER_H

#include "net-utils.h"
#include <cstring>  // Include the missing header
#include <iostream> // Include the missing header
#include <string>
#include <unistd.h>

class ArgumentParser {
  private:
    const int argc;
    char *const *argv;

    int opt;

    bool typeSet = false;
    bool ipSet = false;

    void usage();

  public:
    ProtocolType type;
    std::string ip;
    uint16_t port = 4567;
    uint16_t timeout = 250;
    uint8_t retries = 3;

    ArgumentParser(int argc, char *argv[]);
    void print();
    void parse();
};

#endif // ARG_PARSER_H