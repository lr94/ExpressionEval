AC_INIT(libexpreval, version-0.1)
AM_INIT_AUTOMAKE([subdir-objects])
AC_PREFIX_DEFAULT(/usr)
AC_PROG_RANLIB

dnl AC_MSG_NOTICE([Hello, world.])

AC_LANG(C)
AC_PROG_CC

AC_ARG_ENABLE([shared],
   AS_HELP_STRING([[[--enable-shared]]],
      [Compile shared library]),
   shared="yes",
   shared="no")
AM_CONDITIONAL(MAKE_SHARED_LIBRARY, test "$shared" = "yes")

AC_PROG_AWK
AC_PROG_SED
AC_CHECK_FUNCS([pow])
AC_CHECK_FUNCS([sqrt])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strndup])
AC_CHECK_HEADERS([malloc.h])
AC_CHECK_HEADERS([sys/time.h])


AC_SEARCH_LIBS([pow], [m], [], [
	AC_MSG_ERROR([Unable to find standard math library])
])

if test "$host" = ""; then
	arch=$(uname -m | $AWK '{print tolower($0)}')
	os=$(uname -s | $AWK '{print tolower($0)}')
else
	arch=$(echo $host | $AWK '{print $1}' FS=-)
	os=$(echo $host | $AWK '{print $(NF-1)}' FS=-)
fi;
if test "$arch" = "i386"; then
	arch="i686";
elif test "$arch" = "i486"; then
	arch="i686";
elif test "$arch" = "i586"; then
	arch="i686";
fi;

# Supported OSs
if test "$os" = "mingw32"; then
	os="win"; # TODO win port
	AC_SUBST(SHARED_EXTENSION, "dll")
elif test "$os" = "linux"; then
	AC_SUBST(SHARED_EXTENSION, "so")
else
	AC_MSG_ERROR([Unsupported os $os])
fi;
AC_SUBST(OS, $os)
AC_SUBST(ARCH, $arch)

# Supported architectures

if ! echo "i686 x86_64" | $GREP -w $arch > /dev/null; then
	AC_MSG_ERROR([Architecture $arch is not supported])
fi;

if echo "i686 x86_64" | $GREP -w $arch > /dev/null; then
	AC_CHECK_PROG(HAVE_NASM, nasm, yes, no)
	dnl AC_MSG_RESULT($HAVE_NASM)
	if test "$HAVE_NASM" = "no"; then
		AC_MSG_ERROR([nasm not found])
	fi;
	AC_SUBST(AS, nasm)
fi;

AM_CONDITIONAL(ARCH_i686, test "$arch" = "i686")
AM_CONDITIONAL(ARCH_x86_64, test "$arch" = "x86_64")

AC_OUTPUT(Makefile)
dnl Anna
