#include <QApplication>
#include <QInputDialog>
#include <QLineEdit>
#include "ChatWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    //small popup boxes asking for the server IP and username BEFORE the chat window opens
    bool ok;
    QString ip = QInputDialog::getText(nullptr, "Your LAN Party", "Server IP:",
                                       QLineEdit::Normal, "127.0.0.1", &ok);
    if (!ok || ip.isEmpty()) return 0;//user pressed Cancel, so just quit

    QString username = QInputDialog::getText(nullptr, "Your LAN Party", "Username:",
                                             QLineEdit::Normal, "", &ok);
    if (!ok || username.isEmpty()) return 0;

    ChatWindow window(username, ip);//pass both into the window
    window.show();

    return app.exec();
}