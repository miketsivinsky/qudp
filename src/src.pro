TEMPLATE    = lib
TARGET      = qudp
CONFIG     += staticlib
TOPDIR      = ..

include($$TOPDIR/common.pri)
include($$TOPDIR/build.pri)

HEADERS    += \
               $$TOPDIR/include/tqueue.h          \
               $$TOPDIR/include/SysUtils.h        \
               $$TOPDIR/include/SysUtilsQt.h      \
               $$TOPDIR/include/qudp_lib.h        \
               $$TOPDIR/include/UDP_defs.h        \
               $$TOPDIR/include/rawstreamtester.h \
               qudp.h
SOURCES    += \
               qudp.cpp                           \
               qudp_lib.cpp                       \
               rawstreamtester.cpp

QT         += testlib network

DEFINES += ENA_FW_QT

win32 {
    DEFINES += ENA_WIN_API
    LIBS += -lws2_32
}
