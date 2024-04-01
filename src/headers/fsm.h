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
    FSM(State initialState, MailBox &mb);
    void run();
};

#endif // FSM_H
