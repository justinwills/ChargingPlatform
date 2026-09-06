QT       += core network sql
QT       -= gui

CONFIG   += c++17 console
CONFIG   -= app_bundle

TARGET = ProtocolTest

INCLUDEPATH += \
    ../db \
    ../protocol \
    ../server \
    ../client

SOURCES += \
    protocol_test_main.cpp \
    ../db/database.cpp \
    ../protocol/protocolcodec.cpp \
    ../server/requestdispatcher.cpp \
    ../server/clientthread.cpp \
    ../server/serverlistener.cpp \
    ../client/clientconnection.cpp

HEADERS += \
    ../db/database.h \
    ../protocol/protocolcodec.h \
    ../server/requestdispatcher.h \
    ../server/clientthread.h \
    ../server/serverlistener.h \
    ../client/clientconnection.h
