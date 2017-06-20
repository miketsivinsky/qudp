set LIB_DIR=libs

@if not exist %LIB_DIR% mkdir %LIB_DIR%
copy bin\release\qudp.lib %LIB_DIR%
