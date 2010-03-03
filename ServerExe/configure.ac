dnl Process this file with autoconf to produce a configure script.
AC_INIT([seamlessrdp], [1.0], [rdesktop-users@lists.sourceforge.net])
AC_CONFIG_SRCDIR(main.c)

AM_INIT_AUTOMAKE
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PROG_CC
if test "$GCC" = yes; then
    CFLAGS="$CFLAGS -Wall"
fi
AC_PROG_LIBTOOL

AC_LIBTOOL_WIN32_DLL

AC_OUTPUT(Makefile)
