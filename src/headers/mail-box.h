#ifndef MAIL_BOX_H
#define MAIL_BOX_H

#include "net-utils.h"
#include "pipes.h"
#include "utils.h"
#include <arpa/inet.h> // For htons
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
    };

    struct ConfirmMessage {
        int RefMessageID;
    };

    struct AuthMessage {
        uint16_t MessageID;
        std::string Username;
        std::string DisplayName;
        std::string Secret;
    };

    struct JoinMessage {
        int MessageID;
        std::string ChannelID;
        std::string DisplayName;
    };

    struct ErrorMessage {
        uint MessageID;
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
    bool addToMailQueue = true;
    bool sigint = false;

    int getMessageID() const {
        return std::visit([this](const auto &msg) -> int {
            using T = std::decay_t<decltype(msg)>;
            if constexpr (std::is_same_v<T, Mail::ConfirmMessage>) {
                return msg.RefMessageID;
            } else if constexpr (std::is_same_v<T, Mail::AuthMessage> ||
                                 std::is_same_v<T, Mail::JoinMessage> ||
                                 std::is_same_v<T, Mail::ErrorMessage> ||
                                 std::is_same_v<T, Mail::ByeMessage> ||
                                 std::is_same_v<T, Mail::TextMessage> ||
                                 std::is_same_v<T, Mail::ReplyMessage>) {
                return msg.MessageID;
            } else {
                // Handle the case where neither MessageID nor RefMessageID exist
                return -1; // Or throw an exception, or use a default value
            }
        },
                          data);
    }

    int getRefMessageID() const {
        return std::visit([this](const auto &msg) -> int {
            using T = std::decay_t<decltype(msg)>;
            if constexpr (std::is_same_v<T, Mail::ConfirmMessage>) {
                return msg.RefMessageID;
            } else if constexpr (std::is_same_v<T, Mail::ReplyMessage>) {
                return msg.RefMessageID;
            } else {
                // Handle the case where RefMessageID does not exist
                return -1; // Or throw an exception, or use a default value
            }
        },
                          data);
    }

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
    uint16_t sequenceUDPNumber = 0; // Tells the sequence number for UDP messages, so listener can send a reply with CONFIRM

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
    int srvMsgId = -1;

    MailBox(ProtocolType protocolType, Pipe *pipe = nullptr)
        : delivery_service(protocolType), notifyListenerPipe(pipe) {}

    Mail waitMail() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return !incomingMails.empty(); });
        Mail mail = incomingMails.front();
        incomingMails.pop();
        return mail;
    }

    Pipe *getNotifyListenerPipe() {
        return notifyListenerPipe;
    }

    Mail getOutgoingMail() {
        std::lock_guard<std::mutex> lock(mtx);
        Mail mail = outgoingMails.front();
        outgoingMails.pop();
        return mail;
    }

    void addMail(const Mail &mail) {
        {
            if (mail.addToMailQueue == false) {
                return;
            }
            std::lock_guard<std::mutex> lock(mtx);
            incomingMails.push(mail);
            printGreen("Mail added to incomingMails");
        }
        cv.notify_one();
    }

    void sendMail(Mail &mail) {
        if (mail.addToMailQueue == false) {
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
                arg.MessageID = sequenceUDPNumber++;
            }
        },
                   mail.data);

        {
            std::lock_guard<std::mutex> lock(mtx); // Lock the mutex for thread safety
            printGreen("Adding mail to outgoingMails");
            outgoingMails.push(mail); // Add mail to the outgoingMails queue
        }
        // Send '1' to notifyListenerPipe to indicate a new mail has been sent
        if (notifyListenerPipe != nullptr) {
            printGreen("Sending notification to listener");
            notifyListenerPipe->write("1");
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
            printYellow(std::string("Auth: ") + authMsg.Username + ", " + authMsg.Secret + ", " + authMsg.DisplayName);
            mail.type = Mail::MessageType::AUTH;
            mail.data = authMsg;
            mail.addToMailQueue = true;
            displayName = authMsg.DisplayName;
        } else if (command == "/join") {
            Mail::JoinMessage joinMsg;
            iss >> joinMsg.ChannelID;
            printYellow(std::string("Join: ") + joinMsg.ChannelID + ", " + joinMsg.DisplayName);
            joinMsg.DisplayName = displayName;
            mail.type = Mail::MessageType::JOIN;
            mail.data = joinMsg;
            mail.addToMailQueue = true;
        } else if (command == "/rename") {
            std::string newDisplayName;
            iss >> newDisplayName;
            printYellow(std::string("Rename: ") + newDisplayName);
            mail.addToMailQueue = false;
            displayName = newDisplayName;
        } else if (command == "/help") {
            printYellow("Help");
            mail.addToMailQueue = false;
        } else if (command == "/print") {
            printYellow("Print");
            printMails();
            mail.addToMailQueue = false;
        } else {
            mail.type = Mail::MessageType::MSG;
            Mail::TextMessage textMsg;
            textMsg.ToSend = true;
            textMsg.MessageID = sequenceUDPNumber;
            textMsg.DisplayName = displayName;
            textMsg.MessageContent = line;
            mail.data = textMsg;
            mail.addToMailQueue = true;

            printYellow(std::string("Message: ") + textMsg.DisplayName + ": " + textMsg.MessageContent);
        }
        return true;
    }

    bool writeUDPMail(char **buffer, Mail &mail) {
        const char *current = *buffer;
        mail.type = static_cast<Mail::MessageType>(static_cast<unsigned char>(*current));
        current++;
        switch (mail.type) {
        case Mail::MessageType::CONFIRM:
            printYellow("Confirm message");
            mail.data = Mail::ConfirmMessage(readUInt16(&current));
            mail.addToMailQueue = false;
            break;
        case Mail::MessageType::BYE:
            printYellow("Bye message");
            if (delivery_service == ProtocolType::TCP) {
                mail.data = Mail::ByeMessage{readUInt16(&current)};
                mail.addToMailQueue = false;
            } else {
                mail.data = Mail::ByeMessage{readUInt16(&current)};
                mail.addToMailQueue = true;
            }
            break;
        case Mail::MessageType::ERR: {
            printYellow("Error message");
            Mail::ErrorMessage errMsg;
            errMsg.MessageID = readUInt16(&current);
            errMsg.DisplayName = readString(&current);
            errMsg.MessageContent = readString(&current);
            mail.data = errMsg;
            mail.addToMailQueue = true;
            break;
        }
        case Mail::MessageType::REPLY: {
            printYellow("Reply message");
            Mail::ReplyMessage replyMsg;
            replyMsg.MessageID = readUInt16(&current);
            replyMsg.Result = *current;
            current++;
            replyMsg.RefMessageID = readUInt16(&current);
            replyMsg.MessageContent = readString(&current);
            mail.data = replyMsg;
            mail.addToMailQueue = true;
            break;
        }
        case Mail::MessageType::AUTH: {
            printYellow("Auth message");
            Mail::AuthMessage authMsg;
            authMsg.MessageID = readUInt16(&current);
            authMsg.Username = readString(&current);
            authMsg.DisplayName = readString(&current);
            authMsg.Secret = readString(&current);
            mail.data = authMsg;
            mail.addToMailQueue = true;
            break;
        }
        case Mail::MessageType::JOIN: {
            printYellow("Join message");
            Mail::JoinMessage joinMsg;
            joinMsg.MessageID = readUInt16(&current);
            joinMsg.ChannelID = readString(&current);
            joinMsg.DisplayName = readString(&current);
            mail.data = joinMsg;
            mail.addToMailQueue = true;
            break;
        }
        case Mail::MessageType::MSG: {
            printYellow("Text message");
            Mail::TextMessage srvMsg;
            srvMsg.MessageID = readUInt16(&current);
            srvMsg.DisplayName = readString(&current);
            srvMsg.MessageContent = readString(&current);
            mail.data = srvMsg;
            mail.addToMailQueue = true;
            break;
        }
        default:
            printRed("Invalid message type");
            return false;
            break;
        }

        return true;
    }

    bool writeTCPMail(char **buffer, Mail &mail) {
        std::string str(*buffer); // Construct a string from the char* buffer.
        std::istringstream iss(str);
        std::string msgType;

        iss >> msgType;
        printBlue("Message type: " + msgType);

        if (msgType == "BYE") {
            mail.type = Mail::MessageType::BYE;
        } else if (msgType == "ERR") {
            Mail::ErrorMessage errMsg;
            std::string from;
            std::string is;

            iss >> from >> errMsg.DisplayName >> is;
            std::getline(iss, errMsg.MessageContent);                // Use getline to read the rest of the line
            errMsg.MessageContent = errMsg.MessageContent.substr(1); // Remove leading space

            if (from != "FROM" || is != "IS" || errMsg.MessageContent.empty()) {
                printRed("Invalid message format");
                return false;
            }

            mail.type = Mail::MessageType::ERR;
            mail.data = errMsg;
            mail.addToMailQueue = true;

        } else if (msgType == "REPLY") {
            printYellow("Reply message");
            Mail::ReplyMessage replyMsg;
            std::string response;
            std::string is;

            iss >> response >> is;
            std::getline(iss, replyMsg.MessageContent);                  // Use getline to read the rest of the line
            replyMsg.MessageContent = replyMsg.MessageContent.substr(1); // Remove leading space

            if ((response != "OK" && response != "NOK") || is != "IS") {
                printRed("Invalid message format");
                printRed("Response: " + response + ", IS: " + is + ", MessageContent: " + replyMsg.MessageContent);
                return false;
            }

            if (response == "OK") {
                replyMsg.Result = true;
            } else {
                replyMsg.Result = false;
            }

            mail.type = Mail::MessageType::REPLY;
            mail.data = replyMsg;
            mail.addToMailQueue = true;
        } else if (msgType == "AUTH") {
            Mail::AuthMessage authMsg;
            std::string as;
            std::string usg;

            iss >> authMsg.Username >> as >> authMsg.Secret >> usg >> authMsg.DisplayName;
            if (as != "AS" || usg != "USING") {
                printRed("Invalid message format");
                return false;
            }

            mail.type = Mail::MessageType::AUTH;
            mail.data = authMsg;
            mail.addToMailQueue = true;

        } else if (msgType == "JOIN") {
            Mail::JoinMessage joinMsg;
            std::string as;

            iss >> joinMsg.ChannelID >> as >> joinMsg.DisplayName;
            if (as != "AS") {
                printRed("Invalid message format");
                return false;
            }

            mail.type = Mail::MessageType::JOIN;
            mail.data = joinMsg;
            mail.addToMailQueue = true;
        } else if (msgType == "MSG") {
            Mail::TextMessage srvMsg;
            std::string from;
            std::string is;

            iss >> from >> srvMsg.DisplayName >> is;
            std::getline(iss, srvMsg.MessageContent);                // Use getline to read the rest of the line
            srvMsg.MessageContent = srvMsg.MessageContent.substr(1); // Remove leading space

            if (from != "FROM" || is != "IS") {
                printRed("Invalid message format");
                return false;
            }

            mail.type = Mail::MessageType::MSG;
            mail.data = srvMsg;
            mail.addToMailQueue = true;
        } else {
            printRed("Invalid message type");
            return false;
        }
        return true;
    }

    bool writeMail(char **buffer, Mail &mail) {
        printBlue("Writing mail");
        if (delivery_service == ProtocolType::UDP) {
            return writeUDPMail(buffer, mail);
        } else {
            return writeTCPMail(buffer, mail);
        }
    }

    bool writeMail(Mail::MessageType msgType, Mail &mail, int refMessageID = -1) {

        switch (msgType) {
        case Mail::MessageType::ERR:
            mail.type = Mail::MessageType::ERR;
            mail.data = Mail::ErrorMessage{sequenceUDPNumber, displayName, "MessageContent"};
            printYellow("Error message");
            break;
        case Mail::MessageType::BYE:
            mail.type = Mail::MessageType::BYE;
            mail.data = Mail::ByeMessage{sequenceUDPNumber};
            printYellow("Bye message");
            break;
        case Mail::MessageType::CONFIRM:
            mail.type = Mail::MessageType::CONFIRM;
            mail.data = Mail::ConfirmMessage{refMessageID};
            printYellow("Confirm message");
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
    MailSerializer(bool isUDP = true)
        : isUDP(isUDP) {}

    std::vector<char> serialize(const Mail &mail) {
        // Reset buffer for new serialization
        buffer.clear();

        // Serialize MessageType
        if (isUDP) {
            // Assuming mail.type is a char or can be statically cast to char
            buffer.push_back(static_cast<char>(mail.type));
        } else {
            // String to represent the message type
            std::string messageType;

            // Determine the message type string
            switch (mail.type) {
            case Mail::MessageType::CONFIRM:
                messageType = "CONFIRM ";
                break;
            case Mail::MessageType::AUTH:
                messageType = "AUTH ";
                break;
            case Mail::MessageType::JOIN:
                messageType = "JOIN ";
                break;
            case Mail::MessageType::ERR:
                messageType = "ERR ";
                break;
            case Mail::MessageType::BYE:
                messageType = "BYE ";
                break;
            case Mail::MessageType::MSG:
                messageType = "MSG ";
                break;
            case Mail::MessageType::REPLY:
                messageType = "REPLY ";
                break;
            default:
                messageType = "ERR ";
                break;
            }

            // Insert the messageType into the buffer
            buffer.insert(buffer.end(), messageType.begin(), messageType.end());
        }

        // Serialize MessageData
        std::visit(*this, mail.data);

        // if TCP swap last space for /r/n
        if (!isUDP) {
            buffer.pop_back();
            buffer.push_back('\r');
            buffer.push_back('\n');
        }

        return buffer;
    }

    void operator()(const Mail::ConfirmMessage &msg) {
        serialize(msg.RefMessageID);
    }

    void operator()(const Mail::AuthMessage &msg) {
        serialize(msg.MessageID);
        serialize(msg.Username);
        if (!isUDP) {
            // Correctly insert "AS " into the buffer
            std::string asStr = "AS ";
            buffer.insert(buffer.end(), asStr.begin(), asStr.end());
        }
        serialize(msg.DisplayName);
        if (!isUDP) {
            // Correctly insert "USING " into the buffer
            std::string usingStr = "USING ";
            buffer.insert(buffer.end(), usingStr.begin(), usingStr.end());
        }
        serialize(msg.Secret);
    }

    void operator()(const Mail::JoinMessage &msg) {
        serialize(msg.MessageID);
        serialize(msg.ChannelID);
        if (!isUDP) {
            std::string asStr = "AS ";
            buffer.insert(buffer.end(), asStr.begin(), asStr.end());
        }
        serialize(msg.DisplayName);
    }

    void operator()(const Mail::ErrorMessage &msg) {
        if (!isUDP) {
            std::string asStr = "FROM ";
            buffer.insert(buffer.end(), asStr.begin(), asStr.end());
        }
        serialize(msg.MessageID);
        serialize(msg.DisplayName);
        if (!isUDP) {
            std::string asStr = "IS ";
            buffer.insert(buffer.end(), asStr.begin(), asStr.end());
        }
        serialize(msg.MessageContent);
    }

    void operator()(const Mail::ByeMessage &msg) {
        serialize(msg.MessageID);
    }

    void operator()(const Mail::TextMessage &msg) {
        if (!isUDP) {
            std::string asStr = "FROM ";
            buffer.insert(buffer.end(), asStr.begin(), asStr.end());
        }
        serialize(msg.MessageID);
        serialize(msg.DisplayName);
        if (!isUDP) {
            std::string asStr = "IS ";
            buffer.insert(buffer.end(), asStr.begin(), asStr.end());
        }
        serialize(msg.MessageContent);
    }

    void operator()(const Mail::ReplyMessage &msg) {
        serialize(msg.MessageID);
        serialize(msg.Result);
        if (!isUDP) {
            std::string asStr = "NULL";
            if (msg.Result) {
                asStr = "OK ";
            } else {
                asStr = "NOK ";
            }
            buffer.insert(buffer.end(), asStr.begin(), asStr.end());

            asStr = "IS ";
            buffer.insert(buffer.end(), asStr.begin(), asStr.end());
        }
        serialize(msg.RefMessageID);
        serialize(msg.MessageContent);
    }

  private:
    bool isUDP = true;

    void serialize(uint16_t value) {
        switch (isUDP) {
        case true: {
            uint16_t bigEndianValue = htons(value); // Convert to big endian
            auto data = reinterpret_cast<const char *>(&bigEndianValue);
            buffer.insert(buffer.end(), data, data + sizeof(bigEndianValue));
            printBlue("Serialized int (big endian): " + std::to_string(value));
            break;
        }
        case false: {
            break;
        }
        }
    }

    void serialize(const std::string &str) {
        switch (isUDP) {
        case true:
            buffer.insert(buffer.end(), str.begin(), str.end());
            buffer.push_back('\0'); // Null-terminate string
            printBlue("Serialized string: " + str);
            break;
        case false:
            buffer.insert(buffer.end(), str.begin(), str.end());
            buffer.push_back(' '); // sp
            printBlue("Serialized string: " + str);
        }
    }
};

#endif // MAIL_BOX_H