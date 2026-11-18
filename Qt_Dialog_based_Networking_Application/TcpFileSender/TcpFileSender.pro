QT += core gui widgets network

CONFIG += c++17

TEMPLATE = app
TARGET = TcpFileSender

msvc:QMAKE_CXXFLAGS += /utf-8

SOURCES += \
    main.cpp \
    tcpfilesender.cpp

HEADERS += \
    tcpfilesender.h