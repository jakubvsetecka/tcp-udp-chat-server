#ifndef MAIL_BOX_H
#define MAIL_BOX_H

#include "net-utils.h"
#include "pipes.h"
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
        AUTH,
        JOIN,
        ERR,
        BYE,
        SRV_MSG,
        USR_MSG,
        REPLY,
        NOT_REPLY
    };

    struct AuthMessage {
        std::string Username;
        std::string DisplayName;
        std::string Secret;
    };

    struct JoinMessage {
        int ChannelID;
        std::string DisplayName;
    };

    struct ErrorMessage {
        std::string DisplayName;
        std::string MessageContent;
    };

    // BYE does not need a struct since it has no associated data

    struct TextMessage {
        std::string DisplayName;
        std::string MessageContent;
    };

    struct ReplyMessage {
        bool IsReply; // true for REPLY, false for !REPLY
        std::string MessageContent;
    };

    using MessageData = std::variant<
        AuthMessage,
        JoinMessage,
        ErrorMessage,
        // No struct needed for BYE
        TextMessage,
        ReplyMessage>;

    MessageType type;
    MessageData data;

    void printMail() const {
        std::visit([this](const auto &arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, AuthMessage>) {
                std::cout << "AUTH: " << arg.Username << ", " << arg.DisplayName << ", " << arg.Secret << std::endl;
            } else if constexpr (std::is_same_v<T, JoinMessage>) {
                std::cout << "JOIN: " << arg.ChannelID << ", " << arg.DisplayName << std::endl;
            } else if constexpr (std::is_same_v<T, ErrorMessage>) {
                std::cout << "ERR: " << arg.DisplayName << ", " << arg.MessageContent << std::endl;
            } else if (type == MessageType::BYE) {
                std::cout << "BYE" << std::endl;
            } else if constexpr (std::is_same_v<T, TextMessage>) {
                std::cout << (type == MessageType::SRV_MSG ? "SRV_MSG: " : "USR_MSG: ")
                          << arg.DisplayName << ", " << arg.MessageContent << std::endl;
            } else if constexpr (std::is_same_v<T, ReplyMessage>) {
                std::cout << (arg.IsReply ? "REPLY: " : "NOT_REPLY: ") << arg.MessageContent << std::endl;
            }
        },
                   data);
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

    Mail writeMail(std::string line) {
        std::istringstream iss(line);
        std::string command;
        iss >> command;

        Mail mail;

        if (command == "/auth") {
            Mail::AuthMessage authMsg;
            iss >> authMsg.Username >> authMsg.Secret >> authMsg.DisplayName;
            mail.type = Mail::MessageType::AUTH;
            mail.data = authMsg;
        } else if (command == "/join") {
            Mail::JoinMessage joinMsg;
            iss >> joinMsg.ChannelID;
            // Assuming DisplayName needs to be set for JOIN message as well
            // If not, remove the following line
            iss >> joinMsg.DisplayName;
            mail.type = Mail::MessageType::JOIN;
            mail.data = joinMsg;
        } else {
            // Handle invalid command or other message types
        }

        return mail;
    }
    Mail writeMail(char *buffer); // implementation remains
    Mail writeMail(Mail::MessageType msgType) {
        Mail mail;
        switch (msgType) {
        case Mail::MessageType::ERR:
            mail.type = Mail::MessageType::ERR;
            mail.data = Mail::ErrorMessage{displayName, "Invalid message"};
            break;
        case Mail::MessageType::BYE:
            mail.type = Mail::MessageType::BYE;
            break;
        default: // TODO: shouldnt happen
            mail.type = Mail::MessageType::ERR;
            mail.data = Mail::ErrorMessage{displayName, "Invalid message"};
            break;
        }
        return mail;
    }
};

#endif // MAIL_BOX_H