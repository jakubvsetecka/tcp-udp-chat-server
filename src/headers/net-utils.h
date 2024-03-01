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

class Mail {
  public:
    int type;
    std::vector<std::string> args; // Use vector for dynamic array
    bool sent;
    bool delivered;
};

class MailBox {
  public:
    ProtocolType delivery_service;
    std::vector<Mail> mails;

    void addMail(const Mail &mail) {
        mails.push_back(mail);
    }
    Mail getMail() {
        if (mails.empty()) {
            Mail mail;
            mail.type = -1;
            return mail;
        }
        Mail mail = mails.front();
        mails.erase(mails.begin());
        return mail;
    }
};

#endif // NET_UTILS_H