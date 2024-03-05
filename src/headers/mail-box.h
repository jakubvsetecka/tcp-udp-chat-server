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
        SRV_MSG = 0x04,
        USR_MSG = 0x04,
        REPLY = 0x01,
        NOT_REPLY = 0x01,
        CONFIRM = 0x00,
        DO_NOT_ADD_TO_QUEUE,
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
    std::string displayName;
    int sequenceUDPNumber; // Tells the sequence number for UDP messages, so listener can send a reply with CONFIRM

    uint16_t readUInt16(const char **buffer) {
        uint16_t value = **buffer | (*(*buffer + 1) << 8);
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

    void addMail(const Mail &mail) {
        {
            if (mail.type == Mail::MessageType::DO_NOT_ADD_TO_QUEUE) {
                return;
            }
            std::lock_guard<std::mutex> lock(mtx);
            incomingMails.push(mail);
        }
        cv.notify_one();
    }

    void sendMail(const Mail &mail) {
        {
            std::lock_guard<std::mutex> lock(mtx); // Lock the mutex for thread safety
            outgoingMails.push(mail);              // Add mail to the outgoingMails queue
        }
        // Send '1' to notifyListenerPipe to indicate a new mail has been sent
        if (notifyListenerPipe != nullptr) {
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
            // Handle invalid command or other message types
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
        case Mail::MessageType::BYE:
            mail.data = Mail::ByeMessage{readUInt16(&current)};
            break;
        case Mail::MessageType::ERR: {
            Mail::ErrorMessage errMsg;
            errMsg.MessageID = readUInt16(&current);
            errMsg.DisplayName = readString(&current);
            errMsg.MessageContent = readString(&current);
            mail.data = errMsg;
            break; // Add a break statement to prevent fall-through to the next case
        }
        case Mail::MessageType::REPLY: {
            Mail::ReplyMessage replyMsg;
            replyMsg.MessageID = readUInt16(&current);
            replyMsg.Result = *current;
            current++;
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
        case Mail::MessageType::SRV_MSG: {
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

#endif // MAIL_BOX_H