#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([gst-rtsp-server-wfd],[0.0.1])
AC_CONFIG_SRCDIR([src/rtsp-client.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_HEADER_STDBOOL
AC_HEADER_STDC
AC_HEADER_TIME
AC_PROG_GCC_TRADITIONAL

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_INT64_T
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_CHECK_FUNCS([gettimeofday memset strchr strstr])

PKG_CHECK_MODULES(MMTA, mm-ta)
AC_SUBST(MMTA_CFLAGS)
AC_SUBST(MMTA_LIBS)

PKG_CHECK_MODULES(MMCOMMON,mm-common)
AC_SUBST(MMCOMMON_CFLAGS)
AC_SUBST(MMCOMMON_LIBS)

PKG_CHECK_MODULES(GLIB, glib-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(GST, gstreamer-0.10 >= 0.10)
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

PKG_CHECK_MODULES(GST_PLUGIN_BASE, gstreamer-plugins-base-0.10 >= 0.10)
AC_SUBST(GST_PLUGIN_BASE_CFLAGS)
AC_SUBST(GST_PLUGIN_BASE_LIBS)

PKG_CHECK_MODULES(GST_INTERFACE, gstreamer-interfaces-0.10 >= 0.10)
AC_SUBST(GST_INTERFACE_CFLAGS)
AC_SUBST(GST_LIBS)

PKG_CHECK_MODULES(GST_APP, gstreamer-app-0.10 >= 0.10)
AC_SUBST(GST_APP_CFLAGS)
AC_SUBST(GST_APP_LIBS)

PKG_CHECK_MODULES(INIPARSER, iniparser)
AC_SUBST(INIPARSER_CFLAGS)
AC_SUBST(INIPARSER_LIBS)

PKG_CHECK_MODULES(X11, x11)
AC_SUBST(X11_CFLGAS)
AC_SUBST(X11_LIBS)

PKG_CHECK_MODULES(XAU, xau)
AC_SUBST(XAU_CFLAGS)
AC_SUBST(XAU_LIBS)

PKG_CHECK_MODULES(XDMCP, xdmcp)
AC_SUBST(XDMCP_CFLAGS)
AC_SUBST(XDMCP_LIBS)

PKG_CHECK_MODULES(XEXT, xext)
AC_SUBST(XEXT_CFLAGS)
AC_SUBST(XEXT_LIBS)

PKG_CHECK_MODULES(LIBDRI2, libdri2)
AC_SUBST(LIBDRI2_CFLAGS)
AC_SUBST(LIBDRI2_LIBS)

PKG_CHECK_MODULES(LIBDRM, libdrm)
AC_SUBST(LIBDRM_CFLAGS)
AC_SUBST(LIBDRM_LIBS)

PKG_CHECK_MODULES(UTILX, utilX)
AC_SUBST(UTILX_CFLAGS)
AC_SUBST(UTILX_LIBS)

PKG_CHECK_MODULES(VCONF, vconf)
AC_SUBST(VCONF_CFLAGS)
AC_SUBST(VCONF_LIBS)

PKG_CHECK_MODULES(APPFW_APPLICATION, capi-appfw-application)
AC_SUBST(APPFW_APPLICATION_CFLAGS)
AC_SUBST(APPFW_APPLICATION_LIBS)

PKG_CHECK_MODULES(MM_WFD_COMMON, mm-wfd-common)
AC_SUBST(MM_WFD_COMMON_CFLAGS)
AC_SUBST(MM_WFD_COMMON_LIBS)
# for testsuite

AC_ARG_ENABLE(sdk, AC_HELP_STRING([--enable-sdk], [sdk build]),
	 [
		case "${enableval}" in
		yes) IS_SDK=yes ;;
		no)  IS_SDK=no ;;
		  *)   AC_MSG_ERROR(bad value ${enableval} for --enable-sdk) ;;
		esac
	],
[IS_SDK=no])
AM_CONDITIONAL([IS_SDK], [test "x$IS_SDK" = "xyes"])


AC_ARG_ENABLE(hdcp, AC_HELP_STRING([--enable-hdcp], [HDCP]),
[
 case "${enableval}" in
         yes) ENABLE_HDCP=yes ;;
         no)  ENABLE_HDCP=no ;;
         *)   AC_MSG_ERROR(bad value ${enableval} for --enable-hdcp) ;;
 esac
 ],
 [ENABLE_HDCP=no])
AM_CONDITIONAL(ENABLE_HDCP, test "x$ENABLE_HDCP" = "xyes")
if [test "x$ENABLE_HDCP" = "xyes"];
then
PKG_CHECK_MODULES(HDCP, hdcp2)
AC_SUBST(HDCP_CFLAGS)
AC_SUBST(HDCP_LIBS)
fi


AC_ARG_ENABLE(wfd-extended-features, AC_HELP_STRING([--enable-wfd-extended-features], [samsung wfd spec]),
[
 case "${enableval}" in
         yes) ENABLE_WFD_EXTENDED_FEATURES=yes ;;
         no)  ENABLE_WFD_EXTENDED_FEATURES=no ;;
         *)   AC_MSG_ERROR(bad value ${enableval} for --enable-wfd-extended-features) ;;
 esac
 ],
 [ENABLE_WFD_EXTENDED_FEATURES=no])
AM_CONDITIONAL(ENABLE_WFD_EXTENDED_FEATURES, test "x$ENABLE_WFD_EXTENDED_FEATURES" = "xyes")

AC_ARG_ENABLE([qc-specific], AC_HELP_STRING([--enable-qc-specific], [Enable QC changes for WFD]),
	[
	 case "${enableval}" in
	  yes) IS_QC_SPECIFIC=yes ;;
	  no)  IS_QC_SPECIFIC=no ;;
	  *)   AC_MSG_ERROR(bad value ${enableval} for --enable-qc-specific) ;;
	 esac
	],
	[IS_QC_SPECIFIC=no])
AM_CONDITIONAL([IS_QC_SPECIFIC], [test "x$IS_QC_SPECIFIC" = "xyes"])


AC_CONFIG_FILES([Makefile
		 src/Makefile
		 test/Makefile
		 gst-rtsp-server-wfd.pc
])
AC_OUTPUT
