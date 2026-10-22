#include "ChatWindow.h"
#include "Protocol.h"   //shared sendMessage() / recvMessage() helpers (length-prefixed messages)
#include <QMessageBox>
#pragma comment(lib, "ws2_32.lib")

ChatWindow::ChatWindow(const QString& username, const QString& serverIp, QWidget* parent)
    : QWidget(parent), username(username), serverIp(serverIp), connected(false) {
    //initializer list: saves the popup answers into the class's own variables, starts as "not connected"
    setWindowTitle("Your LAN Party - " + username);
    resize(400, 500);

    messageDisplay = new QTextEdit(this);
    messageDisplay->setReadOnly(true);  // user can't type directly into the chat history

    messageInput = new QLineEdit(this);
    messageInput->setPlaceholderText("Type a message...");
    sendButton = new QPushButton("Send", this);

    // arrange input box + send button side by side
    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->addWidget(messageInput);
    inputLayout->addWidget(sendButton);

    // stack the chat history above the input row
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(messageDisplay);
    mainLayout->addLayout(inputLayout);

    // connect the button's "clicked" signal to our onSendClicked function
    connect(sendButton, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
    connect(messageInput, &QLineEdit::returnPressed, this, &ChatWindow::onSendClicked);

    //connect our own messageReceived signal to the slot that updates the display
    //Qt::QueuedConnection -> the slot runs later on the GUI thread, which is what makes it safe
    connect(this, &ChatWindow::messageReceived, this, &ChatWindow::onMessageReceived, Qt::QueuedConnection);

    connectToServer();  //connect to the server as soon as the window is created
}

//sets up Winsock and connects clientSocket to the server, same sequence as the console client
void ChatWindow::connectToServer() {
    WSADATA wsaData;//Create variable of WSADATA datatype
    WSAStartup(MAKEWORD(2, 2), &wsaData);//Initializing Winsock, version 2.2 of API

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);//create a TCP socket (IPv4, TCP, auto protocol)

    sockaddr_in serverAddr{};//struct to hold the server's IP+port, zero-initialized
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(5000);//convert port 5000 into network byte order
    inet_pton(AF_INET, serverIp.toStdString().c_str(), &serverAddr.sin_addr);
    //now uses the IP typed in the popup instead of a hardcoded 127.0.0.1
    //toStdString().c_str() -> QString -> std::string -> raw char* that inet_pton() expects

    //connect() returns 0 on success, non-zero if it failed
    if (::connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
        messageDisplay->append("[Failed to connect to server]");
        return;//stop here, no point logging in or starting the receiver thread
    }

    sendMessage(clientSocket, "LOGIN|" + username.toStdString());//first message must always be LOGIN|username
    connected = true;
    messageDisplay->append("[Connected to server]");

    receiverThread = std::thread(&ChatWindow::receiveLoop, this);
    //start listening for incoming messages on a background thread
}

//runs on the background thread: waits for messages from the server and hands them to the GUI
void ChatWindow::receiveLoop() {
    std::string msg;
    while (recvMessage(clientSocket, msg)) {//blocks until a message arrives, false when disconnected
        QString text;
        size_t a = msg.find('|');//separates TYPE from the rest
        if (a == std::string::npos) {
            text = QString::fromStdString(msg);
        } else {
            std::string type = msg.substr(0, a);
            std::string rest = msg.substr(a + 1);
            if (type == "CHAT") {
                size_t b = rest.find('|');//CHAT messages look like CHAT|sender|text
                text = QString::fromStdString(rest.substr(0, b)) + ": " + QString::fromStdString(rest.substr(b + 1));
                        } else if (type == "PM") {
                size_t b = rest.find('|');
                text = "(private) " + QString::fromStdString(rest.substr(0, b)) + ": " + QString::fromStdString(rest.substr(b + 1));
            } else if (type == "USERS") {
                text = "Online: " + QString::fromStdString(rest);
            } else {
                text = "[" + QString::fromStdString(rest) + "]";//SYSTEM / OK / ERROR messages
            }
        }
        emit messageReceived(text);//fire the signal, GUI thread will display it
    }
    emit messageReceived("[Disconnected from server]");
}

//runs on the GUI thread (triggered by the messageReceived signal)
void ChatWindow::onMessageReceived(const QString& text) {
    messageDisplay->append(text);
}

void ChatWindow::onSendClicked() {
    QString text = messageInput->text();
    if (text.isEmpty()) return;

    if (!connected) {
        messageDisplay->append("[Not connected to server]");
        return;
    }

    //commands are only checked once we know we're connected
    if (text == "/users") {
        sendMessage(clientSocket, "USERS|");
        messageInput->clear();
        return;
    }
    if (text.startsWith("/w ")) {
        QString rest = text.mid(3);
        int sp = rest.indexOf(' ');
        if (sp <= 0) {
            messageDisplay->append("[Usage: /w name message]");
            return;
        }
        sendMessage(clientSocket, "PM|" + rest.left(sp).toStdString() + "|" + rest.mid(sp + 1).toStdString());
        messageInput->clear();
        return;
    }

    messageDisplay->append("Me: " + text);  // show it in the chat window

    //convert QString (Qt's string type) into std::string, since sendMessage() expects a C++ string, not a Qt type
    sendMessage(clientSocket, "CHAT|" + text.toStdString());
    //"CHAT|" prefix tells the server what kind of message this is

    messageInput->clear();                   // clear the input box
}

//destructor: runs automatically when the ChatWindow object is destroyed (window closed)
ChatWindow::~ChatWindow() {
    if (connected) {
        shutdown(clientSocket, SD_BOTH);//wakes up recv() in the receiver thread so its loop can end
        if (receiverThread.joinable()) receiverThread.join();//wait for that thread to finish before cleaning up
    }
    closesocket(clientSocket);//close our connection to the server
    WSACleanup();//tell Windows we're done using networking
}