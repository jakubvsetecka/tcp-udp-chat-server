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
                    std::cout << "ERR: Unexpected message: " << std::get<Mail::TextMessage>(mail.data).MessageContent << std::endl;
                    break;
                case Mail::MessageType::JOIN:
                    std::cout << "ERR: Unexpected message " << std::endl;
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
                        std::cout << "Success: " << std::get<Mail::ReplyMessage>(mail.data).MessageContent << std::endl;
                        state = OPEN;
                    } else {
                        std::cout << "Failure: " << std::get<Mail::ReplyMessage>(mail.data).MessageContent << std::endl;
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
                    std::cout << "ERR: Unexpected message: " << std::get<Mail::TextMessage>(mail.data).MessageContent << std::endl;
                    break;
                case Mail::MessageType::JOIN:
                    std::cout << "ERR: Unexpected message " << std::endl;
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
                    } else {
                        const auto &msg = std::get<Mail::TextMessage>(mail.data);
                        std::cout << msg.DisplayName << ": " << msg.MessageContent << std::endl;
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
                std::cerr << "Unexpected state" << std::endl;
                break;
            }
        }
    }
};

#endif // FSM_H