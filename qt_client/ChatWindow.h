#pragma once
#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <winsock2.h>   //needed for SOCKET type and networking functions
#include <ws2tcpip.h>   //needed for inet_pton()
#include <thread>       //std::thread, for the receiver thread

class ChatWindow : public QWidget {
    Q_OBJECT  // required macro: enables Qt's signal/slot system for this class

public:
    ChatWindow(const QString& username, const QString& serverIp, QWidget* parent = nullptr);
    ~ChatWindow();  //destructor: runs automatically when the window closes, used to clean up the socket

signals:
    void messageReceived(const QString& text);
    //a "signal" the receiver thread fires when a message arrives
    //widgets can ONLY be touched from the GUI thread, so the thread doesn't touch the display directly,
    //it fires this signal and Qt safely runs onMessageReceived() on the GUI thread

private slots:
    void onSendClicked();  // runs when the Send button is clicked
    void onMessageReceived(const QString& text);  //runs on the GUI thread, adds the text to the display

private:
    QTextEdit* messageDisplay;  // shows the chat history
    QLineEdit* messageInput;    // where the user types a message
    QPushButton* sendButton;    // the Send button

    SOCKET clientSocket;        //our connection to the server, stored so onSendClicked() can reuse it
    QString username;           //name typed in the startup popup
    QString serverIp;           //server IP typed in the startup popup
    bool connected;             //true only if connect() + login succeeded
    std::thread receiverThread; //runs receiveLoop() in the background so the window never freezes

    void connectToServer();     //sets up and connects clientSocket, called once when the window opens
    void receiveLoop();         //runs on receiverThread, waits for messages from the server forever
};