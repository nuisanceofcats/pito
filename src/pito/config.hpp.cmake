#ifndef _PITO_CONFIG_HPP_
#define _PITO_CONFIG_HPP_

#define PITO_PREFIX  "@PREFIX@"
#define PITO_LIB_DIR "@LIB_DIR@"
#define PITO_SHARE_DIR "@SHARE_DIR@"
#define PITO_CONFIG_DIR "@CONFIG_DIR@"
#define PITO_OFF64_TYPE @OFF64_TYPE@
#define PITO_SHARED_LIB_FILE_EXTENSION "@CMAKE_SHARED_LIBRARY_SUFFIX@"
#cmakedefine PITO_APPLE
#ifndef PITO_APPLE
#define PITO_LD_PRELOAD      "LD_PRELOAD"
#define PITO_LD_LIBRARY_PATH "LD_LIBRARY_PATH"
#else
#define PITO_LD_PRELOAD "DYLD_INSERT_LIBRARIES"
#define PITO_LD_LIBRARY_PATH "DYLD_LIBRARY_PATH"
#endif

#endif
