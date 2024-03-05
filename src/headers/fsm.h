#ifndef FSM_H
#define FSM_H

#include "mail-box.h"

class FSM {

    enum State {
        START,
        AUTH,
        OPEN,
        ERROR,
        END
    };

  private:
    State state;
    MailBox &mailbox;
    Mail mail;

  public:
    FSM(State initialState, MailBox &mb)
        : state(initialState), mailbox(mb) {}

    void run() {
        while (1) {
            switch (state) {
            case START:
                mail = mailbox.waitMail();
                switch (mail.type) {
                case Mail::MessageType::AUTH:
                    mailbox.sendMail(mail);
                    state = AUTH;
                    break;
                default:
                    mailbox.writeMail(Mail::MessageType::ERR, mail);
                    mailbox.sendMail(mail);
                    state = ERROR;
                    break;
                }
                break;
            case AUTH:
                mail = mailbox.waitMail();
                switch (mail.type) {
                case Mail::MessageType::REPLY:
                    if (std::get<Mail::ReplyMessage>(mail.data).Result == true) {
                        std::cout << "Authentication successful" << std::endl;
                        state = OPEN;
                    } else {
                        std::cout << "Authentication failed" << std::endl;
                        std::cout << "Enter username: ";
                        mail = mailbox.waitMail();
                        if (mail.type == Mail::MessageType::AUTH) {
                            mailbox.sendMail(mail);
                        } else {
                            state = ERROR;
                        }
                    }
                    break;
                case Mail::MessageType::ERR:
                    mailbox.writeMail(Mail::MessageType::ERR, mail);
                    mailbox.sendMail(mail);
                    state = END;
                    break;
                default:
                    mailbox.writeMail(Mail::MessageType::ERR, mail);
                    mailbox.sendMail(mail);
                    state = ERROR;
                    break;
                }
                break;
            case OPEN:
                mail = mailbox.waitMail();
                switch (mail.type) {
                case Mail::MessageType::MSG:
                    if (std::get<Mail::TextMessage>(mail.data).ToSend == true) {
                        mailbox.sendMail(mail);
                    } else {
                        const auto &msg = std::get<Mail::TextMessage>(mail.data);
                        std::cout << "DisplayName: " << msg.DisplayName << ", MessageContent: " << msg.MessageContent << std::endl;
                    }
                    break;
                case Mail::MessageType::REPLY:
                    state = OPEN;
                    break;
                case Mail::MessageType::ERR:
                    mailbox.writeMail(Mail::MessageType::ERR, mail);
                    mailbox.sendMail(mail);
                    state = END;
                    break;
                case Mail::MessageType::BYE:
                    state = END;
                    break;
                case Mail::MessageType::JOIN:
                    mailbox.sendMail(mail);
                    break;
                default:
                    mailbox.writeMail(Mail::MessageType::ERR, mail);
                    mailbox.sendMail(mail);
                    state = ERROR;
                    break;
                }
                break;
            case ERROR:
                mailbox.writeMail(Mail::MessageType::BYE, mail);
                mailbox.sendMail(mail);
                state = END;
                break;
            case END:
                state = END;
                return; // TODO: maybe just break?
            default:
                // handle unexpected state
                std::cerr << "Unexpected state" << std::endl;
                break;
            }
        }
    }
};

#endif // FSM_H