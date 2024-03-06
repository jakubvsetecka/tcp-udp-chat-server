#ifndef MAIL_BOX_H
#define MAIL_BOX_H

#include "net-utils.h"
#include "pipes.h"
#include "utils.h"
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <variant>

class Mail {
  public:
    enum class MessageType {
        AUTH = 0x02,
        JOIN = 0x03,
        ERR = 0xFE,
        BYE = 0xFF,
        MSG = 0x04,
        REPLY = 0x01,
        CONFIRM = 0x00,
        DO_NOT_ADD_TO_QUEUE = 0x11,
    };

    struct ConfirmMessage {
        int RefMessageID;
    };

    struct AuthMessage {
        int MessageID;
        std::string Username;
        std::string DisplayName;
        std::string Secret;
    };

    struct JoinMessage {
        int MessageID;
        int ChannelID = 0;
        std::string DisplayName;
    };

    struct ErrorMessage {
        int MessageID;
        std::string DisplayName;
        std::string MessageContent;
    };

    struct ByeMessage {
        int MessageID;
    };

    struct TextMessage {
        bool ToSend = false;
        int MessageID;
        std::string DisplayName;
        std::string MessageContent;
    };

    struct ReplyMessage {
        int MessageID;
        bool Result; // true for REPLY, false for !REPLY
        int RefMessageID;
        std::string MessageContent;
    };

    using MessageData = std::variant<
        ConfirmMessage,
        AuthMessage,
        JoinMessage,
        ErrorMessage,
        ByeMessage,
        TextMessage,
        ReplyMessage>;

    MessageType type;
    MessageData data;

    void printMail() const {
        // start printg in magenta color
        std::cout << "\033[1;35m";
        std::visit([this](const auto &arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, ConfirmMessage>) {
                std::cout << "CONFIRM: RefMessageID = " << arg.RefMessageID << std::endl;
            } else if constexpr (std::is_same_v<T, AuthMessage>) {
                std::cout << "AUTH: MessageID = " << arg.MessageID << ", Username = " << arg.Username << ", DisplayName = " << arg.DisplayName << ", Secret = " << arg.Secret << std::endl;
            } else if constexpr (std::is_same_v<T, JoinMessage>) {
                std::cout << "JOIN: MessageID = " << arg.MessageID << ", ChannelID = " << arg.ChannelID << ", DisplayName = " << arg.DisplayName << std::endl;
            } else if constexpr (std::is_same_v<T, ErrorMessage>) {
                std::cout << "ERR: MessageID = " << arg.MessageID << ", DisplayName = " << arg.DisplayName << ", MessageContent = " << arg.MessageContent << std::endl;
            } else if constexpr (std::is_same_v<T, ByeMessage>) {
                std::cout << "BYE: MessageID = " << arg.MessageID << std::endl;
            } else if constexpr (std::is_same_v<T, TextMessage>) {
                std::cout << "TEXT: MessageID = " << arg.MessageID << ", DisplayName = " << arg.DisplayName << ", MessageContent = " << arg.MessageContent << std::endl;
            } else if constexpr (std::is_same_v<T, ReplyMessage>) {
                std::cout << "REPLY: MessageID = " << arg.MessageID << ", Result = " << (arg.Result ? "Success" : "Failure") << ", RefMessageID = " << arg.RefMessageID << ", MessageContent = " << arg.MessageContent << std::endl;
            }
        },
                   data);
        std::cout << "\033[0m"; // reset color
    }
};

class MailBox {

  private:
    ProtocolType delivery_service;
    std::queue<Mail> incomingMails;
    std::queue<Mail> outgoingMails;
    std::mutex mtx;
    std::condition_variable cv;
    Pipe *notifyListenerPipe;
    std::string displayName = "default";
    int sequenceUDPNumber = 0; // Tells the sequence number for UDP messages, so listener can send a reply with CONFIRM

    uint16_t readUInt16(const char **buffer) {
        uint16_t value = (*(*buffer + 1)) | (**buffer << 8);
        (*buffer) += 2;
        return value;
    }

    std::string readString(const char **buffer) {
        std::string result;
        while (**buffer != 0) {
            result.push_back(**buffer);
            (*buffer)++;
        }
        (*buffer)++; // Skip the terminating zero byte
        return result;
    }

  public:
    MailBox(ProtocolType protocolType, Pipe *pipe = nullptr)
        : delivery_service(protocolType), notifyListenerPipe(pipe) {}

    Mail waitMail() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return !incomingMails.empty(); });
        Mail mail = incomingMails.front();
        incomingMails.pop();
        return mail;
    }

    Mail getOutgoingMail() {
        std::lock_guard<std::mutex> lock(mtx);
        Mail mail = outgoingMails.front();
        outgoingMails.pop();
        return mail;
    }

    void addMail(const Mail &mail) {
        {
            if (mail.type == Mail::MessageType::DO_NOT_ADD_TO_QUEUE) {
                return;
            }
            std::lock_guard<std::mutex> lock(mtx);
            incomingMails.push(mail);
            printGreen("Mail added to incomingMails");
        }
        cv.notify_one();
    }

    void sendMail(Mail &mail) {
        if (mail.type == Mail::MessageType::DO_NOT_ADD_TO_QUEUE) {
            return;
        }

        // Set the MessageID for the mail if applicable
        std::visit([&](auto &arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, Mail::AuthMessage> ||
                          std::is_same_v<T, Mail::JoinMessage> ||
                          std::is_same_v<T, Mail::ErrorMessage> ||
                          std::is_same_v<T, Mail::ByeMessage> ||
                          std::is_same_v<T, Mail::TextMessage> ||
                          std::is_same_v<T, Mail::ReplyMessage>) {
                // TODO: Generate or retrieve a suitable MessageID
                arg.MessageID = sequenceUDPNumber++;
            }
        },
                   mail.data);

        {
            std::lock_guard<std::mutex> lock(mtx); // Lock the mutex for thread safety
            outgoingMails.push(mail);              // Add mail to the outgoingMails queue
        }
        // Send '1' to notifyListenerPipe to indicate a new mail has been sent
        if (notifyListenerPipe != nullptr) {
            printGreen("Sending notification to listener");
            notifyListenerPipe->write("1"); // Send a string with '1'
        }
    }

    bool writeMail(std::string line, Mail &mail) {
        std::istringstream iss(line);
        std::string command;
        iss >> command;

        // printBlue(std::string("Command: ") + command);

        if (command == "/auth") {
            Mail::AuthMessage authMsg;
            iss >> authMsg.Username >> authMsg.Secret >> authMsg.DisplayName;
            // printYellow(std::string("Auth: ") + authMsg.Username + ", " + authMsg.Secret + ", " + authMsg.DisplayName);
            mail.type = Mail::MessageType::AUTH;
            mail.data = authMsg;
        } else if (command == "/join") {
            Mail::JoinMessage joinMsg;
            iss >> joinMsg.ChannelID;
            // Assuming DisplayName needs to be set for JOIN message as well
            // If not, remove the following line
            iss >> joinMsg.DisplayName;
            // printYellow(std::string("Join: ") + std::to_string(joinMsg.ChannelID) + ", " + joinMsg.DisplayName);
            mail.type = Mail::MessageType::JOIN;
            mail.data = joinMsg;
        } else if (command == "/rename") {
            std::string newDisplayName;
            iss >> newDisplayName;
            // printYellow(std::string("Rename: ") + newDisplayName);
            mail.type = Mail::MessageType::DO_NOT_ADD_TO_QUEUE;
            displayName = newDisplayName;
        } else if (command == "/help") {
            // printYellow("Help");
            mail.type = Mail::MessageType::DO_NOT_ADD_TO_QUEUE;
        } else if (command == "/print") {
            printYellow("Print");
            printMails();
            mail.type = Mail::MessageType::DO_NOT_ADD_TO_QUEUE;
        } else {
            mail.type = Mail::MessageType::MSG;
            Mail::TextMessage textMsg;
            textMsg.ToSend = true;
            textMsg.MessageID = sequenceUDPNumber;
            textMsg.DisplayName = displayName;
            textMsg.MessageContent = line;
            mail.data = textMsg;
        }

        return true;
    }

    bool writeMail(char **buffer, Mail &mail) {
        printBlue("Writing mail");

        const char *current = *buffer;
        mail.type = static_cast<Mail::MessageType>(static_cast<unsigned char>(*current));
        current++;
        switch (mail.type) {
        case Mail::MessageType::CONFIRM:
            mail.data = Mail::ConfirmMessage(readUInt16(&current));
            break;
        case Mail::MessageType::BYE:
            mail.data = Mail::ByeMessage{readUInt16(&current)};
            break;
        case Mail::MessageType::ERR: {
            Mail::ErrorMessage errMsg;
            errMsg.MessageID = readUInt16(&current);
            errMsg.DisplayName = readString(&current);
            errMsg.MessageContent = readString(&current);
            mail.data = errMsg;
            break;
        }
        case Mail::MessageType::REPLY: {
            Mail::ReplyMessage replyMsg;
            replyMsg.MessageID = readUInt16(&current);
            replyMsg.Result = *current;
            current++;
            replyMsg.RefMessageID = readUInt16(&current);
            replyMsg.MessageContent = readString(&current);
            mail.data = replyMsg;
            break;
        }
        case Mail::MessageType::AUTH: {
            Mail::AuthMessage authMsg;
            authMsg.MessageID = readUInt16(&current);
            authMsg.Username = readString(&current);
            authMsg.DisplayName = readString(&current);
            authMsg.Secret = readString(&current);
            mail.data = authMsg;
            break;
        }
        case Mail::MessageType::JOIN: {
            Mail::JoinMessage joinMsg;
            joinMsg.MessageID = readUInt16(&current);
            joinMsg.ChannelID = std::stoi(readString(&current));
            joinMsg.DisplayName = readString(&current);
            mail.data = joinMsg;
            break;
        }
        case Mail::MessageType::MSG: {
            Mail::TextMessage srvMsg;
            srvMsg.MessageID = readUInt16(&current);
            srvMsg.DisplayName = readString(&current);
            srvMsg.MessageContent = readString(&current);
            mail.data = srvMsg;
            break;
        }
        default:
            printRed("Invalid message type");
            return false;
            break;
        }

        return true;
    }
    bool writeMail(Mail::MessageType msgType, Mail &mail) {

        switch (msgType) {
        case Mail::MessageType::ERR:
            mail.type = Mail::MessageType::ERR;
            mail.data = Mail::ErrorMessage{sequenceUDPNumber, displayName, "MessageContent"};
            break;
        case Mail::MessageType::BYE:
            mail.type = Mail::MessageType::BYE;
            mail.data = Mail::ByeMessage{sequenceUDPNumber};
            break;
        default: // TODO: shouldnt happen
            mail.type = Mail::MessageType::ERR;
            mail.data = Mail::ErrorMessage{sequenceUDPNumber, displayName, "MessageContent"};
            break;
        }
        return true;
    }

    void printMails() {
        std::queue<Mail> tempQueue = incomingMails;
        while (!tempQueue.empty()) {
            tempQueue.front().printMail();
            tempQueue.pop();
        }
    }
};

class MailSerializer {
    std::vector<char> buffer;

  public:
    std::vector<char> serialize(const Mail &mail) {
        // Reset buffer for new serialization
        buffer.clear();

        // Serialize MessageType
        buffer.push_back(static_cast<char>(mail.type));

        // Serialize MessageData
        std::visit(*this, mail.data);

        return buffer;
    }

    void operator()(const Mail::ConfirmMessage &msg) {
        serialize(msg.RefMessageID);
    }

    void operator()(const Mail::AuthMessage &msg) {
        serialize(msg.MessageID);
        serialize(msg.Username);
        serialize(msg.DisplayName);
        serialize(msg.Secret);
    }

    void operator()(const Mail::JoinMessage &msg) {
        serialize(msg.MessageID);
        serialize(msg.ChannelID);
        serialize(msg.DisplayName);
    }

    void operator()(const Mail::ErrorMessage &msg) {
        serialize(msg.MessageID);
        serialize(msg.DisplayName);
        serialize(msg.MessageContent);
    }

    void operator()(const Mail::ByeMessage &msg) {
        serialize(msg.MessageID);
    }

    void operator()(const Mail::TextMessage &msg) {
        serialize(msg.MessageID);
        serialize(msg.DisplayName);
        serialize(msg.MessageContent);
    }

    void operator()(const Mail::ReplyMessage &msg) {
        serialize(msg.MessageID);
        serialize(msg.Result);
        serialize(msg.RefMessageID);
        serialize(msg.MessageContent);
    }

  private:
    void serialize(int value) {
        auto data = reinterpret_cast<const char *>(&value);
        buffer.insert(buffer.end(), data, data + sizeof(value));
    }

    void serialize(const std::string &str) {
        buffer.insert(buffer.end(), str.begin(), str.end());
        buffer.push_back('\0'); // Null-terminate string
    }
};

#endif // MAIL_BOX_H