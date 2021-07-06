AC_INIT(phevtdisp,[0.01])
AC_CONFIG_SRCDIR([configure.ac])

AM_INIT_AUTOMAKE
AC_PROG_CXX(CC g++)
AC_ENABLE_STATIC(NO)
AC_PROG_LIBTOOL

dnl if test $ac_cv_prog_gcc = yes; then
dnl  CXXFLAGS="$CXXFLAGS -Wall -Werror"
dnl fi
case $CXX in
  clang++)
   CXXFLAGS="$CXXFLAGS -Wall -Werror -Wno-macro-redefined"
  ;;
  *g++)
   CXXFLAGS="$CXXFLAGS -Wall -Werror"
  ;;
esac

CINTDEFS=" -noIncludePaths  -inlineInputHeader "
AC_SUBST(CINTDEFS)

AC_OUTPUT( \
	   Makefile \
	   interface/Makefile \
	   display/Makefile \
	   modules/Makefile )
