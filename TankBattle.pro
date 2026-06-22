QT       += core gui widgets

TARGET = TankBattle
TEMPLATE = app

CONFIG += c++11

SOURCES += \
    main.cpp \
    gameengine.cpp \
    gamewidget.cpp

HEADERS += \
    gameengine.h \
    gamewidget.h

# Qt 5.12 compatibility
win32:CONFIG(release, debug|release): LIBS += -luser32
