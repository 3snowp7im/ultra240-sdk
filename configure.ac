AC_INIT([ultra240-sdk], [0.0.1])
PKG_CHECK_MODULES([JSON], [jsoncpp])
PKG_CHECK_MODULES([PNG], [libpng])
PKG_CHECK_MODULES([YAML], [yaml-cpp])
AC_LANG_PUSH([C++])
AC_CHECK_HEADER([png++/png.hpp], [], [AC_MSG_ERROR([Missing libpng++])])
AC_CHECK_HEADER([rapidxml/rapidxml.hpp], [], [AC_MSG_ERROR([Missing rapidxml])])
AC_LANG_POP([C++])
AM_INIT_AUTOMAKE
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_FILES([
  Makefile
  src/ultra-sdk/Makefile
  src/ultra-sdk-posix/Makefile
  src/ultra-sdk-img/Makefile
  src/ultra-sdk-sheet/Makefile
  src/ultra-sdk-tileset/Makefile
  src/ultra-sdk-world/Makefile
  ultra240-sdk.pc
])
AC_PROG_CXX
LT_INIT
AC_OUTPUT
