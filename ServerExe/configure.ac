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

os_is_win32=0
os_is_win64=0
case "$host" in
    i?86-*-mingw*)
        os_is_win32=1
        ;;
    x86_64-*-mingw*)
        os_is_win64=1
        ;;
esac
AM_CONDITIONAL(OS_IS_WIN32, test "x$os_is_win32" = "x1")
AM_CONDITIONAL(OS_IS_WIN64, test "x$os_is_win64" = "x1")

AC_LIBTOOL_WIN32_DLL

AC_OUTPUT(Makefile)
