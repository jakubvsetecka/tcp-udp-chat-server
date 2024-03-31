#ifndef FSM_H
#define FSM_H

#include "mail-box.h"

enum State {
    START,
    AUTH,
    OPEN,
    ERROR,
    END
};

class FSM {
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
                printYellow("State: START\n");
                mail = mailbox.waitMail();
                switch (mail.type) {
                case Mail::MessageType::AUTH:
                    mailbox.sendMail(mail);
                    state = AUTH;
                    break;
                case Mail::MessageType::ERR:
                    printYellow("State: START: ERR");
                    if (mail.sigint) {
                        mailbox.writeMail(Mail::MessageType::BYE, mail);
                        mailbox.sendMail(mail);
                        state = END;
                    } else {
                        mailbox.writeMail(Mail::MessageType::ERR, mail);
                        mailbox.sendMail(mail);
                        state = ERROR;
                    }
                    break;
                case Mail::MessageType::MSG:
                    std::cerr << "ERR: Unexpected message: " << std::get<Mail::TextMessage>(mail.data).MessageContent << std::endl;
                    break;
                case Mail::MessageType::JOIN:
                    std::cerr << "ERR: Unexpected message " << std::endl;
                    break;
                default:
                    mailbox.writeMail(Mail::MessageType::ERR, mail);
                    mailbox.sendMail(mail);
                    state = ERROR;
                    break;
                }
                break;
            case AUTH:
                printYellow("State: AUTH\n");
                mail = mailbox.waitMail(); // TODO: add flag to specify srv message or user message
                switch (mail.type) {
                case Mail::MessageType::REPLY:
                    if (std::get<Mail::ReplyMessage>(mail.data).Result == true) {
                        state = OPEN;
                    } else {
                        state = AUTH;
                    }
                    break;
                case Mail::MessageType::AUTH:
                    mailbox.sendMail(mail);
                    state = AUTH;
                    break;
                case Mail::MessageType::ERR:
                    printYellow("State: AUTH\n");
                    mailbox.writeMail(Mail::MessageType::BYE, mail);
                    mailbox.sendMail(mail);
                    state = END;
                    break;
                case Mail::MessageType::MSG:
                    std::cerr << "ERR: Unexpected message: " << std::get<Mail::TextMessage>(mail.data).MessageContent << std::endl;
                    break;
                case Mail::MessageType::JOIN:
                    std::cerr << "ERR: Unexpected message. " << std::endl;
                    break;
                default:
                    mailbox.writeMail(Mail::MessageType::ERR, mail);
                    mailbox.sendMail(mail);
                    state = ERROR;
                    break;
                }
                break;
            case OPEN:
                printYellow("State: OPEN\n");
                mail = mailbox.waitMail();
                switch (mail.type) {
                case Mail::MessageType::MSG:
                    printBrown("State: OPEN, Mail: MSG\n");
                    if (std::get<Mail::TextMessage>(mail.data).ToSend == true) {
                        mailbox.sendMail(mail);
                    }
                    break;
                case Mail::MessageType::REPLY:
                    printBrown("State: OPEN, Mail: REPLY\n");
                    state = OPEN;
                    break;
                case Mail::MessageType::ERR:
                    printBrown("State: OPEN, Mail: ERR\n");
                    mailbox.writeMail(Mail::MessageType::BYE, mail);
                    mailbox.sendMail(mail);
                    state = END;
                    break;
                case Mail::MessageType::BYE:
                    printBrown("State: OPEN, Mail: BYE\n");
                    state = END;
                    break;
                case Mail::MessageType::JOIN:
                    printBrown("State: OPEN, Mail: JOIN\n");
                    mailbox.sendMail(mail);
                    break;
                    state = OPEN;
                case Mail::MessageType::AUTH:
                    printBrown("State: OPEN, Mail: AUTH\n");
                    std::cerr << "ERR: User already authenticated." << std::endl;
                    state = OPEN;
                    break;
                default:
                    printBrown("State: OPEN, Mail: DEFAULT\n");
                    mailbox.writeMail(Mail::MessageType::ERR, mail);
                    mailbox.sendMail(mail);
                    state = ERROR;
                    break;
                }
                break;
            case ERROR:
                printYellow("State: ERROR\n");
                mailbox.writeMail(Mail::MessageType::BYE, mail);
                mailbox.sendMail(mail);
                state = END;
                break;
            case END:
                printYellow("State: END\n");
                state = END;
                // TODO: tell listener to stop
                return; // TODO: maybe just break?
            default:
                printYellow("State: DEFAULT\n");
                // handle unexpected state
                throw std::runtime_error("Unexpected state");
                break;
            }
        }
    }
};

#endif // FSM_H