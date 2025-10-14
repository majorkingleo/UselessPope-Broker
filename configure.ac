
dnl @synopsis CXX_FLAGS_CHECK [compiler flags]                                        
dnl @summary check whether compiler supports given C++ flags or not                   
AC_DEFUN([CXX_FLAGS_CHECK],                                                            
[dnl                                                                                  
  AC_MSG_CHECKING([if $CXX supports $1])
  AC_LANG_PUSH([C++])
  ac_saved_cxxflags="$CXXFLAGS"                                                       
  CXXFLAGS="-Werror $1"                                                               
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],                                            
    [AC_MSG_RESULT([yes])],                                                           
    [AC_MSG_ERROR([no])]                                                              
  )                                                                                   
  CXXFLAGS="$ac_saved_cxxflags"                                                       
  AC_LANG_POP([C++])
])

dnl @synopsis LD_FLAGS_CHECK [compiler flags]                                        
dnl @summary check whether linker supports given C++ flags or not                   
AC_DEFUN([LD_FLAGS_CHECK],                                                            
[dnl                                                                                  
  AC_MSG_CHECKING([if $CXX supports $1])
  AC_LANG_PUSH([C++])
  ac_saved_ldflags="$LDFLAGS"                                                       
  LDFLAGS="-Werror $1"                                                               
  AC_LINK_IFELSE([AC_LANG_PROGRAM([])],                                            
    [AC_MSG_RESULT([yes])],                                                           
    [AC_MSG_ERROR([no])]                                                              
  )                                                                                   
  LDFLAGS="$ac_saved_ldflags"                                                       
  AC_LANG_POP([C++])
])


AC_INIT(broker, 0.0.1)
AM_INIT_AUTOMAKE([subdir-objects -Wall])


AC_CANONICAL_HOST
case $host_os in
  *mingw*) mingw=true ;;
  *) mingw=false ;;
esac
AM_CONDITIONAL([MINGW], [test x$mingw = xtrue])

AC_CANONICAL_HOST
case $host_os in
  *cygwin*) cygwin=true ;;
  *) cygwin=false ;;
esac
AM_CONDITIONAL([CYGWIN], [test x$cygwin = xtrue])


AC_PROG_CC
#  po/Makefile.in
AC_CONFIG_FILES([
Makefile
])
AC_PROG_RANLIB
AM_PROG_AR
AC_PROG_CXX
AC_LANG(C++)
AC_PROG_INSTALL

ac_extra_cxxx_flags=""
EXTRA_LDFLAGS=""

CXX_FLAGS_CHECK([-std=gnu++20])
#CXX_FLAGS_CHECK([-fopenmp])


#AM_GNU_GETTEXT([external])
#AM_CONDITIONAL([USE_NLS], [test "x${USE_NLS}" = "xyes"])

AC_OUTPUT
