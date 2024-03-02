#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>

enum class ProtocolType {
    TCP,
    UDP
};

enum class MailType {
    AUTH,
    REPLY,
    NOTREPLY,
    MSG,
    ERR,
    BYE
};

class Mail {

  public:
    MailType type;
    std::vector<std::string> args;
    Mail writeMail(char *buffer);      // implementation remains
    Mail writeMail(MailType mailType); // implementation remains
};

class MailBox {

  private:
    ProtocolType delivery_service;
    std::queue<Mail> incomingMails;
    std::queue<Mail> outgoingMails;
    std::mutex mtx;
    std::condition_variable cv;

  public:
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

    void sendMail(const Mail &mail);       // implementation remains
    void setService(ProtocolType service); // implementation remains
};
