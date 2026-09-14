QT       += core sql
QT       -= gui

CONFIG   += c++17 console
CONFIG   -= app_bundle

TARGET = DatabaseTest

INCLUDEPATH += \
    ../db

SOURCES += \
    db_main.cpp \
    ../db/database.cpp

HEADERS += \
    ../db/database.h
