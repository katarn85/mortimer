#-*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_INIT([sunpinyin],[0.0.1518], [mail@yongsun.me])
AC_PREREQ([2.59])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE

GETTEXT_PACKAGE=sunpinyin
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [The gettext domain])

# Init libtool
AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL
AC_LIB_LTDL
AC_SUBST(LIBTOOL_DEPS)

#AM_MKINSTALLDIRS
AC_CANONICAL_HOST
# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_LANG(C++)
AC_C_CONST
AC_TYPE_SIZE_T
#IT_PROG_INTLTOOL([0.33], [no-xml])
# Checks for libraries.
# Checks for header files.
AC_HEADER_STDC
# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stddef.h stdlib.h string.h wchar.h iconv.h assert.h dlfcn.h fcntl.h getopt.h inttypes.h locale.h libintl.h limits.h locale.h memory.h sys/mman.h sys/param.h sys/stat.h sys/types.h unistd.h])
# Checks for library functions.
AC_CHECK_FUNCS([memset pow sqrt bind_textdomain_codeset dcgettext exp2 getopt_long getpagesize get_opt log2 mmap munmap setlocale strndup])

case $host_cpu in
  *arm* ) TARGET=ARM;;
  * ) TARGET=X86;;
esac
AM_CONDITIONAL(ARM, test x$TARGET = xARM)

ISF_VERSION=1.0.0

PKG_CHECK_MODULES(ISF,[isf >= $ISF_VERSION])
PKG_CHECK_MODULES(SQLITE, [sqlite3 >= 0.0])
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.0])
PKG_CHECK_MODULES(ECORE, [ecore >= 0.0])

# Check if we should build setup module
PKG_CHECK_MODULES(ISF_GTKUTILS,[isf-gtkutils >= $ISF_VERSION],
		  		[ISF_HAS_GTKUTILS=yes],
				[ISF_HAS_GTKUTILS=no])


AC_SUBST(ISF_VERSION)

AM_CONDITIONAL(ISF_BUILD_SETUP, [test "$ISF_HAS_GTKUTILS" = "yes"])

if test "$ISF_HAS_GTKUTILS" = "yes"; then
  ISF_BUILD_SETUP=1
else
  ISF_BUILD_SETUP=0
fi

AC_SUBST(ISF_BUILD_SETUP)

# Checks for library functions.
AC_SUBST(ac_aux_dir)

# libtool option to control which symbols are exported
# right now, symbols starting with _ are not exported
#LIBTOOL_EXPORT_OPTIONS='-export-symbols-regex "^[[^_]].*"'
#AC_SUBST(LIBTOOL_EXPORT_OPTIONS)

# Extra args.
AC_ARG_ENABLE(debug,
	      [  --enable-debug          Turn on debugging],
	      enable_debug=yes,
	      enable_debug=no)
if test "$enable_debug" = "yes"; then
    AC_DEFINE(ENABLE_DEBUG, 1 ,[Define this to enable the debug facility in libscim])
    CFLAGS="$CFLAGS -g"
    CXXFLAGS="$CXXFLAGS -g"
fi

ISF_ICONDIR=`$PKG_CONFIG --variable=icondir scim`
ISF_MODULEDIR=`$PKG_CONFIG --variable=moduledir scim`
ISF_DATADIR=`$PKG_CONFIG --variable=scimdatadir scim`
ISF_LIBDIR=`$PKG_CONFIG --variable=libdir scim`

if test "x$ISF_ICONDIR" = "x"; then
  ISF_ICONDIR=${datadir}/scim/icons
fi

if test "x$ISF_MODULEDIR" = "x"; then
  ISF_MODULEDIR=${libdir}/scim-1.0
fi

if test "x$ISF_DATADIR" = "x"; then
  ISF_DATADIR=${datadir}/scim
fi

if test "x$ISF_LIBDIR" = "x"; then
  ISF_LIBDIR=${libdir}
fi

SUNPINYIN_ICON_DIR=${ISF_ICONDIR}
SUNPINYIN_MODULE_DIR=${ISF_MODULEDIR}
SUNPINYIN_DATA_DIR=${ISF_DATADIR}/ise-engine-sunpinyin
SUNPINYIN_LIB_DIR=${ISF_LIBDIR}

AC_SUBST(SUNPINYIN_ICON_DIR)
AC_SUBST(SUNPINYIN_MODULE_DIR)
AC_SUBST(SUNPINYIN_DATA_DIR)
AC_SUBST(SUNPINYIN_LIBD_IR)


AC_CONFIG_FILES([Makefile
         raw/Makefile
		 src/Makefile
		 src/ime-core/Makefile
		 src/lexicon/Makefile
		 src/pinyin/Makefile
		 src/slm/Makefile
		 wrapper/scim/Makefile
		 wrapper/scim/data/Makefile
		 wrapper/scim/src/Makefile
		 sunpinyin-2.0.pc])
AC_OUTPUT
