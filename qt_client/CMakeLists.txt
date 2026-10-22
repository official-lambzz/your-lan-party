cmake_minimum_required(VERSION 3.16)
project(QtChatClient CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# These let Qt's special features work (signals/slots, UI files, etc.)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

#AUTOMOC → automatically handle Qt's signals and slots system.
#AUTORCC → automatically handle Qt resources like images/icons.
#AUTOUIC → automatically handle Qt .ui files made with Qt Designer.

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(qt_client
    main.cpp
    ChatWindow.cpp
    ChatWindow.h
)

#lets ChatWindow.cpp do #include "Protocol.h", the same shared file the server uses (it lives in ../common)
target_include_directories(qt_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common)

#ws2_32 added here -> needed now that ChatWindow.cpp uses socket()/connect()/send() etc.
target_link_libraries(qt_client PRIVATE Qt6::Widgets ws2_32)