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

    void runFSM() {
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
                    mail = mailbox.writeMail(Mail::MessageType::ERR);
                    mailbox.sendMail(mail);
                    state = ERROR;
                    break;
                }
                break;
            case AUTH:
                mail = mailbox.waitMail();
                switch (mail.type) {
                case Mail::MessageType::REPLY:
                    state = OPEN;
                    break;
                case Mail::MessageType::NOT_REPLY:
                    std::cout << "Authentication failed" << std::endl;
                    std::cout << "Enter username: ";
                    mail = mailbox.waitMail();
                    if (mail.type == Mail::MessageType::AUTH) {
                        mailbox.sendMail(mail);
                    } else {
                        state = ERROR;
                    }
                    break;
                case Mail::MessageType::ERR:
                    mail = mailbox.writeMail(Mail::MessageType::ERR);
                    mailbox.sendMail(mail);
                    state = END;
                    break;
                default:
                    mail = mailbox.writeMail(Mail::MessageType::ERR);
                    mailbox.sendMail(mail);
                    state = ERROR;
                    break;
                }
                break;
            case OPEN:
                mail = mailbox.waitMail();
                switch (mail.type) {
                case Mail::MessageType::SRV_MSG:
                    const auto &msg = std::get<Mail::TextMessage>(mail.data);
                    std::cout << "DisplayName: " << msg.DisplayName << ", MessageContent: " << msg.MessageContent << std::endl;
                    break;
                case Mail::MessageType::USR_MSG:
                    mailbox.sendMail(mail);
                    break;
                case Mail::MessageType::REPLY:
                    state = OPEN;
                    break;
                case Mail::MessageType::NOT_REPLY:
                    state = OPEN;
                    break;
                case Mail::MessageType::ERR:
                    mail = mailbox.writeMail(Mail::MessageType::ERR);
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
                    mail = mailbox.writeMail(Mail::MessageType::ERR);
                    mailbox.sendMail(mail);
                    state = ERROR;
                    break;
                }
                break;
            case ERROR:
                mail = mailbox.writeMail(Mail::MessageType::BYE);
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