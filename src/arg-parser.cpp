#include "arg-parser.h"

void ArgumentParser::usage() {
    std::cout << "Usage: " << argv[0] << " -t <tcp|udp> -s <server_ip> -p <port> -d <display_name> -r <message>" << std::endl;
    exit(1);
}

ArgumentParser::ArgumentParser(int argc, char *argv[])
    : argc(argc), argv(argv) {
    parse();
}

void ArgumentParser::print() {
    std::cout << "Protocol: " << (type == ProtocolType::TCP ? "TCP" : "UDP") << std::endl;
    std::cout << "Server IP: " << ip << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "Timeout: " << timeout << std::endl;
    std::cout << "Retries: " << retries << std::endl;
}

void ArgumentParser::parse() {
    while ((opt = getopt(argc, argv, "t:s:p:d:r:h")) != -1) {
        switch (opt) {
        case 't':
            if (strcmp(optarg, "tcp") == 0) {
                type = ProtocolType::TCP;
            } else if (strcmp(optarg, "udp") == 0) {
                type = ProtocolType::UDP;
            } else {
                usage();
            }
            typeSet = true;
            break;
        case 's':
            ip = optarg;
            ipSet = true;
            break;
        case 'p':
            port = std::stoi(optarg);
            break;
        case 'd':
            timeout = atoi(optarg);
            break;
        case 'r':
            retries = atoi(optarg);
            break;
        case 'h':
            usage();
            break;
        case '?':
        default:
            usage();
            break;
        }
    }
    if (!typeSet || !ipSet) {
        usage();
    }
}