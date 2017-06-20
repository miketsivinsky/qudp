QT       += core
QT       += network
QT       -= gui

TOPDIR      = ../..
include($$TOPDIR/common.pri)
include($$TOPDIR/build.pri)

TARGET = test_rx
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

HEADERS += \
	   ../common/RawStreamTester.h

SOURCES += \
	   main.cpp                      \
	   ../common/RawStreamTester.cpp

LIBS += -lqudp
