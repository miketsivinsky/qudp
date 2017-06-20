MOC_DIR = moc

# TODO: OS dependency
# TODO: compiler dependency -_CRT_SECURE_NO_DEPRECATE   

# DEFINES += OS_WIN LOG_DEBUG_FLUSH
# DEFINES += QT_NO_DEBUG_OUTPUT
 
CONFIG(debug, debug|release) {
   DESTDIR = $$TOPDIR/bin/debug
   DEFINES += QUDP_DEBUG
   OBJECTS_DIR = debug
} else {
   DESTDIR = $$TOPDIR/bin/release
   OBJECTS_DIR = release
}

LIBS += -L$$TOPDIR/libs 
