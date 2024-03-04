#ifndef UTILS_H
#define UTILS_H

#include <iostream>

inline void printRed(const std::string &text) {
    std::cout << "\033[1;31m" << text << "\033[0m"
              << "\n";
}
inline void printGreen(const std::string &text) {
    std::cout << "\033[1;32m" << text << "\033[0m"
              << "\n";
}

inline void printYellow(const std::string &text) {
    std::cout << "\033[1;33m" << text << "\033[0m"
              << "\n";
}

inline void printBlue(const std::string &text) {
    std::cout << "\033[1;34m" << text << "\033[0m"
              << "\n";
}

inline void printMagenta(const std::string &text) {
    std::cout << "\033[1;35m" << text << "\033[0m"
              << "\n";
}

#endif // UTILS_H