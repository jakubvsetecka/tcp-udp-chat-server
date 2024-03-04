#ifndef NET_UTILS_H
#define NET_UTILS_H

#include <string>
#include <vector>

enum class ParamRegex {
    Username = 0,
    ChannelID,
    Secret,
    DisplayName,
    MessageContent
};

inline const std::string regexPatterns[] = {
    "^[A-Za-z0-9-]{1,20}$",   // Username: 1-20 characters, A-z, 0-9, and hyphen.
    "^[A-Za-z0-9-]{1,20}$",   // ChannelID: Same as Username.
    "^[A-Za-z0-9-]{1,128}$",  // Secret: 1-128 characters, A-z, 0-9, and hyphen.
    "^[\\x21-\\x7E]{1,20}$",  // DisplayName: 1-20 printable characters.
    "^[\\x20-\\x7E]{1,1400}$" // MessageContent: 1-1400 printable characters including space.
};

enum class ProtocolType {
    TCP,
    UDP
};

#endif // NET_UTILS_H