TEMPLATE    = lib
TARGET      = qudp
//CONFIG     += console
DEPENDPATH += .
TOPDIR      = ..

include($$TOPDIR/common.pri)
include($$TOPDIR/build.pri)

HEADERS    += \
               $$TOPDIR/include/tqueue.h       \
               $$TOPDIR/include/SysUtils.h     \
               $$TOPDIR/include/SysUtilsQt.h   \
               $$TOPDIR/include/qudp_lib.h     \
               qudp.h
SOURCES    += \
               qudp.cpp                        \
               qudp_lib.cpp
QT         += testlib network

win32 {
    LIBS += -lsetupapi -luuid -ladvapi32 -lws2_32
}
