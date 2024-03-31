#ifndef UTILS_H
#define UTILS_H

#include <iostream>

extern bool verbose;

inline void printRed(const std::string &text) {
    if (verbose)
        std::cout << "\033[1;31m" << text << "\033[0m"
                  << "\n";
}
inline void printGreen(const std::string &text) {
    if (verbose)
        std::cout << "\033[1;32m" << text << "\033[0m"
                  << "\n";
}

inline void printYellow(const std::string &text) {
    if (verbose)
        std::cout << "\033[1;33m" << text << "\033[0m"
                  << "\n";
}

inline void printBlue(const std::string &text) {
    if (verbose)
        std::cout << "\033[1;34m" << text << "\033[0m"
                  << "\n";
}

inline void printMagenta(const std::string &text) {
    if (verbose)
        std::cout << "\033[1;35m" << text << "\033[0m"
                  << "\n";
}

inline void printBrown(const std::string &text) {
    if (verbose)
        std::cout << "\033[0;33m" << text << "\033[0m"
                  << "\n";
}

inline void printCyan(const std::string &text) {
    if (verbose)
        std::cout << "\033[1;36m" << text << "\033[0m"
                  << "\n";
}

inline void printWhite(const std::string &text) {
    if (verbose)
        std::cout << "\033[1;37m" << text << "\033[0m"
                  << "\n";
}

#endif // UTILS_H