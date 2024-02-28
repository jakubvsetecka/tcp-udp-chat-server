#include "net.h"

int main() {
    ProtocolType type = ProtocolType::TCP;
    NetworkConnection connection(type, "127.0.0.1", 8080);
    connection.openConnection();

    Mail mail;
    mail.type = 1;
    mail.args.push_back("Hello, World!\n");
    connection.sendData(mail);

    connection.closeConnection();

    return 0;
}