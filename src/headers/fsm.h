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
                case MailType::AUTH:
                    mailbox.sendMail(mail);
                    state = AUTH;
                    break;
                default:
                    mail = mail.writeMail(MailType::ERR);
                    mailbox.sendMail(mail);
                    state = ERROR;
                    break;
                }
                break;
            case AUTH:
                mail = mailbox.waitMail();
                switch (mail.type) {
                case MailType::REPLY:
                    state = OPEN;
                    break;
                case MailType::NOTREPLY:
                    std::cout << "Authentication failed" << std::endl;
                    std::cout << "Enter username: ";
                    mail = mailbox.waitMail();
                    if (mail.type == MailType::AUTH) {
                        mailbox.sendMail(mail);
                    } else {
                        state = ERROR;
                    }
                    break;
                case MailType::ERR:
                    mail = mail.writeMail(MailType::ERR);
                    mailbox.sendMail(mail);
                    state = END;
                    break;
                default:
                    mail = mail.writeMail(MailType::ERR);
                    mailbox.sendMail(mail);
                    state = ERROR;
                    break;
                }
                break;
            case OPEN:
                mail = mailbox.waitMail();
                switch (mail.type) {
                case MailType::MSG:
                    std::cout << mail.args[0] << std::endl;
                    break;
                case MailType::REPLY:
                    state = OPEN;
                    break;
                case MailType::NOTREPLY:
                    state = OPEN;
                    break;
                case MailType::ERR:
                    mail = mail.writeMail(MailType::ERR);
                    mailbox.sendMail(mail);
                    state = END;
                    break;
                case MailType::BYE:
                    state = END;
                    break;
                default:
                    mail = mail.writeMail(MailType::ERR);
                    mailbox.sendMail(mail);
                    state = ERROR;
                    break;
                }
                break;
            case ERROR:
                mail = mail.writeMail(MailType::BYE);
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
