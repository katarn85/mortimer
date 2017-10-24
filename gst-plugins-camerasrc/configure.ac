AC_INIT(gst-plugins-s5pc210, 1.0)
 
dnl versions of gstreamer and plugins-base
GST_MAJORMINOR=0.10
GST_REQUIRED=0.10.0
GSTPB_REQUIRED=0.10.0
AC_SUBST(GST_MAJORMINOR)

dnl fill in your package name and version here
dnl the fourth (nano) number should be 0 for a release, 1 for CVS,
dnl and 2... for a prerelease

dnl when going to/from release please set the nano correctly !
dnl releases only do Wall, cvs and prerelease does Werror too
AS_VERSION(gst-plugin, GST_PLUGIN_VERSION, 0, 10, 0, 1,
    GST_PLUGIN_CVS="no", GST_PLUGIN_CVS="yes")

dnl AM_MAINTAINER_MODE provides the option to enable maintainer mode
AM_MAINTAINER_MODE

#AM_INIT_AUTOMAKE($PACKAGE, $VERSION)
AM_INIT_AUTOMAKE

#AC_CONFIG_MACRO_DIR([m4])

dnl make aclocal work in maintainer mode
dnl AC_SUBST(ACLOCAL_AMFLAGS, "-I m4")

dnl Add parameters for aclocal
AC_SUBST(ACLOCAL_AMFLAGS, "-I m4 -I common/m4")

AM_CONFIG_HEADER(config.h)

dnl check for tools
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

AC_ARG_ENABLE(aquila, AC_HELP_STRING([--enable-aquila], [using enable-aquila]),
	[
		case "${enableval}" in
			yes) IS_AQUILA=yes ;;
			no) IS_AQUILA=no ;;
			*)  AC_MSG_ERROR(bad value ${enableval} for --enable-aquila) ;;
		esac
	],
	[IS_AQUILA=yes])
AM_CONDITIONAL([IS_AQUILA], [test "x$IS_AQUILA" = "xyes"])

AC_ARG_ENABLE(sdk, AC_HELP_STRING([--enable-sdk], [sdk build]),
	[
		case "${enableval}" in
			yes) IS_SDK=yes ;;
			no)  IS_SDK=no ;;
			*)   AC_MSG_ERROR(bad value ${enableval} for --enable-sdk) ;;
		esac
	]
	[IS_SDK=no])
AM_CONDITIONAL([IS_SDK], [test "x$IS_SDK" = "xyes"])

dnl decide on error flags
AS_COMPILER_FLAG(-Wall, GST_WALL="yes", GST_WALL="no")
                                                                                
if test "x$GST_WALL" = "xyes"; then
   GST_ERROR="$GST_ERROR -Wall"
                                                                                
#   if test "x$GST_PLUGIN_CVS" = "xyes"; then
#     AS_COMPILER_FLAG(-Werror,GST_ERROR="$GST_ERROR -Werror",GST_ERROR="$GST_ERROR")
#   fi
fi

dnl Check for pkgconfig first
AC_CHECK_PROG(HAVE_PKGCONFIG, pkg-config, yes, no)

dnl Give error and exit if we don't have pkgconfig
if test "x$HAVE_PKGCONFIG" = "xno"; then
  AC_MSG_ERROR(you need to have pkgconfig installed !)
fi

dnl Now we're ready to ask for gstreamer libs and cflags
dnl And we can also ask for the right version of gstreamer
PKG_CHECK_MODULES(GST, \
  gstreamer-$GST_MAJORMINOR >= $GST_REQUIRED,
  HAVE_GST=yes,HAVE_GST=no)

dnl Give error and exit if we don't have gstreamer
if test "x$HAVE_GST" = "xno"; then
  AC_MSG_ERROR(you need gstreamer development packages installed !)
fi

dnl append GST_ERROR cflags to GST_CFLAGS
GST_CFLAGS="$GST_CFLAGS $GST_ERROR"

dnl make GST_CFLAGS and GST_LIBS available
PKG_CHECK_MODULES(GST, gstreamer-0.10 >= 0.10)
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

dnl make GST_MAJORMINOR available in Makefile.am
AC_SUBST(GST_MAJORMINOR)

dnl If we need them, we can also use the base class libraries
PKG_CHECK_MODULES(GST_BASE, gstreamer-base-$GST_MAJORMINOR >= $GST_REQUIRED,
                  HAVE_GST_BASE=yes, HAVE_GST_BASE=no)

dnl Give a warning if we don't have gstreamer libs
dnl you can turn this into an error if you need them
if test "x$HAVE_GST_BASE" = "xno"; then
  AC_MSG_NOTICE(no GStreamer base class libraries found (gstreamer-base-$GST_MAJORMINOR))
fi

dnl make _CFLAGS and _LIBS available
AC_SUBST(GST_BASE_CFLAGS)
AC_SUBST(GST_BASE_LIBS)

dnl If we need them, we can also use the gstreamer-plugins-base libraries
PKG_CHECK_MODULES(GSTPB_BASE,
                  gstreamer-plugins-base-$GST_MAJORMINOR >= $GSTPB_REQUIRED,
                  HAVE_GSTPB_BASE=yes, HAVE_GSTPB_BASE=no)

dnl Give a warning if we don't have gstreamer libs
dnl you can turn this into an error if you need them
if test "x$HAVE_GSTPB_BASE" = "xno"; then
  AC_MSG_NOTICE(no GStreamer Plugins Base libraries found (gstreamer-plugins-base-$GST_MAJORMINOR))
fi

dnl make _CFLAGS and _LIBS available
AC_SUBST(GSTPB_BASE_CFLAGS)
AC_SUBST(GSTPB_BASE_LIBS)

dnl If we need them, we can also use the gstreamer-controller libraries
PKG_CHECK_MODULES(GSTCTRL,
                  gstreamer-controller-$GST_MAJORMINOR >= $GSTPB_REQUIRED,
                  HAVE_GSTCTRL=yes, HAVE_GSTCTRL=no)

dnl Give a warning if we don't have gstreamer-controller
dnl you can turn this into an error if you need them
if test "x$HAVE_GSTCTRL" = "xno"; then
  AC_MSG_NOTICE(no GStreamer Controller libraries found (gstreamer-controller-$GST_MAJORMINOR))
fi

dnl make _CFLAGS and _LIBS available
AC_SUBST(GSTCTRL_CFLAGS)
AC_SUBST(GSTCTRL_LIBS)

dnl set the plugindir where plugins should be installed
if test "x${prefix}" = "x$HOME"; then
  plugindir="$HOME/.gstreamer-$GST_MAJORMINOR/plugins"
else
  plugindir="\$(libdir)/gstreamer-$GST_MAJORMINOR"
fi
AC_SUBST(plugindir)

dnl set proper LDFLAGS for plugins
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
AC_SUBST(GST_PLUGIN_LDFLAGS)

PKG_CHECK_MODULES(GST_AUDIO, gstreamer-audio-$GST_MAJORMINOR >= $GST_REQUIRED )


dnl make _CFLAGS and _LIBS available
AC_SUBST(GST_AUDIO_CFLAGS)
AC_SUBST(GST_AUDIO_LIBS)

PKG_CHECK_MODULES(GST_VIDEO, gstreamer-video-$GST_MAJORMINOR >= $GST_REQUIRED)

dnl make _CFLAGS and _LIBS available
AC_SUBST(GST_VIDEO_CFLAGS)
AC_SUBST(GST_VIDEO_LIBS)

dnl gstreamer interface for camerasrc plugin
PKG_CHECK_MODULES(GST_INTERFACES, gstreamer-interfaces-$GST_MAJORMINOR >= $GST_REQUIRED)
AC_SUBST(GST_INTERFACES_CFLAGS)
AC_SUBST(GST_INTERFACES_LIBS)

dnl use time analysis module
PKG_CHECK_MODULES(MMTA, mm-ta)
AC_SUBST(MMTA_CFLAGS)
AC_SUBST(MMTA_LIBS)

AC_OUTPUT(
Makefile
common/Makefile
common/m4/Makefile
camerasrc/Makefile
camerasrc/src/Makefile
)

