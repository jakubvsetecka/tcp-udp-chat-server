#include "arg-parser.h"
#include "net.h"

int main(int argc, char **argv) {

    ArgumentParser args(argc, argv);

    NetworkConnection connection(args.type, args.ip, args.port, args.timeout, args.retries);
    connection.openConnection();

    Mail mail;
    mail.type = 1;
    mail.args.push_back("Hello, World!\n");
    connection.sendData(mail);

    connection.closeConnection();

    return 0;
}
