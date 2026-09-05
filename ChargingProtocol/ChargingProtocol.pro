QT       += core network sql
QT       -= gui

CONFIG   += c++17 console
CONFIG   -= app_bundle

# 这个.pro编译出来是一个"演示+自测"用的console程序（test_main.cpp），
# 会真的起一个服务端+客户端，跑10个请求验证整条链路。
# 正式接入充电用户端/PC服务器端时，按README.md里的说明，把需要的.h/.cpp
# 文件拷贝到各自项目里，不需要这个test_main.cpp和这个.pro本身。

SOURCES += \
    test_main.cpp \
    database.cpp \
    protocolcodec.cpp \
    requestdispatcher.cpp \
    clientthread.cpp \
    serverlistener.cpp \
    clientconnection.cpp

HEADERS += \
    database.h \
    protocolcodec.h \
    requestdispatcher.h \
    clientthread.h \
    serverlistener.h \
    clientconnection.h
