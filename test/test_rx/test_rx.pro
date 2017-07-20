TEMPLATE  = app
TARGET    = test_rx

CONFIG   += console
CONFIG   -= app_bundle
QT       += core network
QT       -= gui

PRJ_DIR   = ../..

include($${PRJ_DIR}/build.pri)

HEADERS += \
           ../common/RawStreamTester.h

SOURCES += \
	     main.cpp \
             ../common/RawStreamTester.cpp

LIBS += -lqudp

win32 {
    LIBS += -lws2_32
}

