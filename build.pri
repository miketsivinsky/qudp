#-------------------------------------------------------------------------------
#-------------------------------------------------------------------------------

#---
CONFIG  -= debug_and_release debug_and_release_target

#---
PRJ_DIR = $${PWD}

#---
if(equals(NO_DEBUG_INFO,1)) {
	DEFINES  += QT_NO_DEBUG_OUTPUT
}

#---
if(defined(QUDP_INC_DIR,var)) {
	INC_DIR = $${QUDP_INC_DIR}
} else {
	INC_DIR = $${PRJ_DIR}/include
}

#---
if(defined(QUDP_OUT_DIR,var)) {
	if(equals(TARGET,"qudp")) {
		OUT_DIR = $${QUDP_OUT_DIR}
        } else {
		OUT_DIR = $${PRJ_DIR}/bin
        }
	LIB_DIR = $${QUDP_OUT_DIR}
} else {
	OUT_DIR = $${PRJ_DIR}/bin
	LIB_DIR = $${PRJ_DIR}/bin
}

#---
if(defined(QUDP_BLD_DIR,var)) {
	BLD_DIR = $${QUDP_BLD_DIR}
} else {
	BLD_DIR = $${PRJ_DIR}/build
}

#---
INCLUDEPATH += .                 \
	       $${PRJ_DIR}/src   \
               $${INC_DIR}

#---
CONFIG(release, debug|release) {
   DESTDIR     = $${OUT_DIR}/release
   OBJECTS_DIR = $${BLD_DIR}/$${TARGET}/release
   LIBS       += -L$${LIB_DIR}/release
} else {
   DESTDIR     = $${OUT_DIR}/debug
   OBJECTS_DIR = $${BLD_DIR}/$${TARGET}/debug
   LIBS       += -L$${LIB_DIR}/debug
}

MOC_DIR  = $${OBJECTS_DIR}/moc
