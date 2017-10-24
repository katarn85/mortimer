
AC_INIT(extension, 1.0)

dnl versions of gstreamer and plugins-base
GST_MAJORMINOR=0.10
GST_REQUIRED=0.10.0
GSTPB_REQUIRED=0.10.0
EFL_REQUIRED=1.0.0

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
AM_CONDITIONAL([IS_VODA_SDK], [test "x$DISTRO" = "xvodafone-sdk"])
AM_CONDITIONAL([ISPROTECTOR_VODA_SDK], [test "x$DISTRO" = "xvodafone-sdk" && test "x$MACHINE" = "xprotector"])
AM_CONDITIONAL([ISVOLANS_VODA_SDK], [test "x$DISTRO" = "xvodafone-sdk" && test "x$MACHINE" = "xvolans"])
AM_CONDITIONAL([ISPROTECTOR_TARGET], [test "x$ARCH" = "xarm" && test "x$MACHINE" = "xprotector"])
AM_CONDITIONAL([ISVOLANS_TARGET], [test "x$ARCH" = "xarm" && test "x$MACHINE" = "xvolans"])

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
#GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\).*'
GST_PLUGIN_LDFLAGS='-module -avoid-version -export-symbols-regex [_]*\(gst_\|Gst\|GST_\|FIR_\|arkamys_malloc\|arkamys_free\).*'
#GST_PLUGIN_LDFLAGS='-module -avoid-version'
AC_SUBST(GST_PLUGIN_LDFLAGS)


PKG_CHECK_MODULES(GST_AUDIO, gstreamer-audio-$GST_MAJORMINOR >= $GST_REQUIRED )


dnl make _CFLAGS and _LIBS available
AC_SUBST(GST_AUDIO_CFLAGS)
AC_SUBST(GST_AUDIO_LIBS)

PKG_CHECK_MODULES(GST_VIDEO, gstreamer-video-$GST_MAJORMINOR >= $GST_REQUIRED)

dnl make _CFLAGS and _LIBS available
AC_SUBST(GST_VIDEO_CFLAGS)
AC_SUBST(GST_VIDEO_LIBS)

PKG_CHECK_MODULES(GST_INTERFACES, gstreamer-interfaces-$GST_MAJORMINOR >= $GST_REQUIRED)
AC_SUBST(GST_INTERFACES_CFLAGS)
AC_SUBST(GST_INTERFACES_LIBS)

dnl xvimagesrc _CFLAGS and _LIB available

PKG_CHECK_MODULES(DRI2, libdri2)
AC_SUBST(DRI2_CFLAGS)
AC_SUBST(DRI2_LIBS)

PKG_CHECK_MODULES(X11, x11)
AC_SUBST(X11_CFLAGS)
AC_SUBST(X11_LIBS)

PKG_CHECK_MODULES(XEXT, xext)
AC_SUBST(XEXT_CFLAGS)
AC_SUBST(XEXT_LIBS)

PKG_CHECK_MODULES(XV, xv)
AC_SUBST(XV_CFLAGS)
AC_SUBST(XV_LIBS)

PKG_CHECK_MODULES(XDAMAGE, xdamage)
AC_SUBST(XDAMAGE_CFLAGS)
AC_SUBST(XDAMAGE_LIBS)

PKG_CHECK_MODULES(DRM, libdrm)
AC_SUBST(DRM_CFLAGS)
AC_SUBST(DRM_LIBS)

PKG_CHECK_MODULES(DRM, libdrm-devel)
AC_SUBST(DRM_DEVEL_CFLAGS)
AC_SUBST(DRM_DEVEL_LIBS)

PKG_CHECK_MODULES(TBM, libtbm)
AC_SUBST(TBM_CFLAGS)
AC_SUBST(TBM_LIBS)

dnl use time analysis module

PKG_CHECK_MODULES(XML2, libxml-2.0)
AC_SUBST(XML2_CFLAGS)
AC_SUBST(XML2_LIBS)

PKG_CHECK_MODULES(NATIVE_BUFFER, native-buffer)
AC_SUBST(NATIVE_BUFFER_CFLAGS)
AC_SUBST(NATIVE_BUFFER_LIBS)

dnl required package for evasimagesink/evaspixmapsink
PKG_CHECK_MODULES(EFL, [
  evas >= $EFL_REQUIRED
  ecore >= $EFL_REQUIRED
  ecore-x >= $EFL_REQUIRED
], [
  AC_SUBST(EFL_CFLAGS)
  AC_SUBST(EFL_LIBS)
], [
  AC_MSG_ERROR([
      You need to install or upgrade the EFL development
      packages on your system. On debian-based systems these are
      libevas-dev and libecore-dev.
      The minimum version required is $EFL_REQUIRED.
  ])
])

dnl *** belows are related to evaspixmapsink plug-ins ***
AG_GST_ARG_WITH_PACKAGE_NAME
AG_GST_ARG_WITH_PACKAGE_ORIGIN

dnl set license and copyright notice
GST_LICENSE="LGPL"
AC_DEFINE_UNQUOTED(GST_LICENSE, "$GST_LICENSE", [GStreamer license])
AC_SUBST(GST_LICENSE)

echo
AC_MSG_NOTICE([Checking libraries for evaspixmapsink plugin])
echo
dnl *** X11 ***
translit(dnm, m, l) AM_CONDITIONAL(USE_X, true)
AG_GST_CHECK_FEATURE(X, [X libraries and plugins],
                  [evaspixmapsink], [
  AC_PATH_XTRA
  ac_cflags_save="$CFLAGS"
  ac_cppflags_save="$CPPFLAGS"
  CFLAGS="$CFLAGS $X_CFLAGS"
  CPPFLAGS="$CPPFLAGS $X_CFLAGS"

  dnl now try to find the HEADER
  AC_CHECK_HEADER(X11/Xlib.h, HAVE_X="yes", HAVE_X="no")

  if test "x$HAVE_X" = "xno"
  then
    AC_MSG_NOTICE([cannot find X11 development files])
  else
    dnl this is much more than we want
    X_LIBS="$X_LIBS $X_PRE_LIBS $X_EXTRA_LIBS"
    dnl AC_PATH_XTRA only defines the path needed to find the X libs,
    dnl it does not add the libs; therefore we add them here
    X_LIBS="$X_LIBS -lX11"
    AC_SUBST(X_CFLAGS)
    AC_SUBST(X_LIBS)
  fi
  AC_SUBST(HAVE_X)
  CFLAGS="$ac_cflags_save"
  CPPFLAGS="$ac_cppflags_save"
])

dnl Check for Xv extension
translit(dnm, m, l) AM_CONDITIONAL(USE_XVIDEO, true)
AG_GST_CHECK_FEATURE(XVIDEO, [X11 XVideo extensions],
                  [evaspixmapsink], [
AG_GST_CHECK_XV
])

dnl check for X Shm
translit(dnm, m, l) AM_CONDITIONAL(USE_XSHM, true)
AG_GST_CHECK_FEATURE(XSHM, [X Shared Memory extension], , [
  if test x$HAVE_X = xyes; then
    AC_CHECK_LIB(Xext, XShmAttach,
		 HAVE_XSHM="yes", HAVE_XSHM="no",
		 $X_LIBS)
    if test "x$HAVE_XSHM" = "xyes"; then
      XSHM_LIBS="-lXext"
    else
      dnl On AIX, it is in XextSam instead, but we still need -lXext
      AC_CHECK_LIB(XextSam, XShmAttach,
		   HAVE_XSHM="yes", HAVE_XSHM="no",
		   $X_LIBS)
      if test "x$HAVE_XSHM" = "xyes"; then
	XSHM_LIBS="-lXext -lXextSam"
      fi
    fi
  fi
], , [
  AC_SUBST(HAVE_XSHM)
  AC_SUBST(XSHM_LIBS)
])

dnl use tbm
PKG_CHECK_MODULES(TBM, libtbm)
AC_SUBST(TBM_CFLAGS)
AC_SUBST(TBM_LIBS)

PKG_CHECK_MODULES(VCONF, vconf)
AC_SUBST(VCONF_CFLAGS)
AC_SUBST(VCONF_LIBS)

dnl PKG_CHECK_MODULES(UDEVMGR, unified-dev-mgr)
dnl AC_SUBST(UDEVMGR_CFLAGS)
dnl AC_SUBST(UDEVMGR_LIBS)

dnl use ext-encodebin --------------------------------------------------------------------------
AC_ARG_ENABLE(ext-encodebin, AC_HELP_STRING([--enable-ext-encodebin], [using encodebin]),
  [
    case "${enableval}" in
      yes) GST_EXT_USE_EXT_ENCODEBIN=yes ;;
      no)  GST_EXT_USE_EXT_ENCODEBIN=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-ext-encodebin) ;;
    esac
  ],
  [GST_EXT_USE_EXT_ENCODEBIN=yes])
AM_CONDITIONAL(GST_EXT_USE_EXT_ENCODEBIN, test "x$GST_EXT_USE_EXT_ENCODEBIN" = "xyes")

dnl use ext-evasimagesink --------------------------------------------------------------------------
AC_ARG_ENABLE(ext-evasimagesink, AC_HELP_STRING([--enable-ext-evasimagesink], [using evasimagesink]),
  [
    case "${enableval}" in
      yes) GST_EXT_USE_EXT_EVASIMAGESINK=yes ;;
      no)  GST_EXT_USE_EXT_EVASIMAGESINK=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-ext-evasimagesink) ;;
    esac
  ],
  [GST_EXT_USE_EXT_EVASIMAGESINK=yes])
AM_CONDITIONAL(GST_EXT_USE_EXT_EVASIMAGESINK, test "x$GST_EXT_USE_EXT_EVASIMAGESINK" = "xyes")

dnl use evaspixmapsink ---------------------------------------------------------------------------
AC_ARG_ENABLE(ext-evaspixmapsink, AC_HELP_STRING([--enable-ext-evaspixmapsink], [using evaspixmapsink]),
  [
    case "${enableval}" in
      yes) GST_EXT_USE_EXT_EVASPIXMAPSINK=yes ;;
      no)  GST_EXT_USE_EXT_EVASPIXMAPSINK=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-ext-evaspixmapsink) ;;
    esac
  ],
  [GST_EXT_USE_EXT_EVASPIXMAPSINK=yes])
AM_CONDITIONAL(GST_EXT_USE_EXT_EVASPIXMAPSINK, test "x$GST_EXT_USE_EXT_EVASPIXMAPSINK" = "xyes")

dnl use ext-xvimagesrc--------------------------------------------------------------------------
AC_ARG_ENABLE(ext-xvimagesrc, AC_HELP_STRING([--enable-ext-xvimagesrc], [using xvimagesrc]),
  [
    case "${enableval}" in
      yes) GST_EXT_USE_EXT_XVIMAGESRC=yes ;;
      no)  GST_EXT_USE_EXT_XVIMAGESRC=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-ext-xvimagesrc) ;;
    esac
  ],
  [GST_EXT_USE_EXT_XVIMAGESRC=yes])
AM_CONDITIONAL(GST_EXT_USE_EXT_XVIMAGESRC, test "x$GST_EXT_USE_EXT_XVIMAGESRC" = "xyes")

AC_ARG_ENABLE(pcmdump, AC_HELP_STRING([--enable-pcmdump], [pcm dump]),
  [
    case "${enableval}" in
      yes) PCM_DUMP_ENABLE=yes ;;
      no)  PCM_DUMP_ENABLE=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-pcmdump) ;;
    esac
  ],
  [PCM_DUMP_ENABLE=no])
AM_CONDITIONAL([PCM_DUMP_ENABLE], [test "x$PCM_DUMP_ENABLE" = "xyes"])

dnl use ext-drmsrc --------------------------------------------------------------------------
AC_ARG_ENABLE(ext-drmsrc, AC_HELP_STRING([--enable-ext-drmsrc], [using drmsrc]),
  [
    case "${enableval}" in
      yes) GST_EXT_USE_EXT_DRMSRC=yes ;;
      no)  GST_EXT_USE_EXT_DRMSRC=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-ext-drmsrc) ;;
    esac
  ],
  [GST_EXT_USE_EXT_DRMSRC=yes])
AM_CONDITIONAL(GST_EXT_USE_EXT_DRMSRC, test "x$GST_EXT_USE_EXT_DRMSRC" = "xyes")

if test "x$GST_EXT_USE_EXT_DRMSRC" = "xyes"; then
	PKG_CHECK_MODULES(DRM_CLIENT, drm-client)
	AC_SUBST(DRM_CLIENT_CFLAGS)
	AC_SUBST(DRM_CLIENT_LIBS)
	PKG_CHECK_MODULES(DRM_TRUSTED, drm-trusted)
	AC_SUBST(DRM_TRUSTED_CFLAGS)
	AC_SUBST(DRM_TRUSTED_LIBS)
fi

dnl use ext-submux --------------------------------------------------------------------------
AC_ARG_ENABLE(ext-submux, AC_HELP_STRING([--enable-ext-submux], [using submux]),
  [
    case "${enableval}" in
      yes) GST_EXT_USE_EXT_SUBMUX=yes ;;
      no)  GST_EXT_USE_EXT_SUBMUX=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-ext-submux) ;;
    esac
  ],
  [GST_EXT_USE_EXT_SUBMUX=yes])
AM_CONDITIONAL(GST_EXT_USE_EXT_SUBMUX, test "x$GST_EXT_USE_EXT_SUBMUX" = "xyes")

dnl use ext-toggle --------------------------------------------------------------------------
AC_ARG_ENABLE(ext-toggle, AC_HELP_STRING([--enable-ext-toggle], [using toggle]),
  [
    case "${enableval}" in
      yes) GST_EXT_USE_EXT_TOGGLE=yes ;;
      no)  GST_EXT_USE_EXT_TOGGLE=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-ext-toggle) ;;
    esac
  ],
  [GST_EXT_USE_EXT_TOGGLE=yes])
AM_CONDITIONAL(GST_EXT_USE_EXT_TOGGLE, test "x$GST_EXT_USE_EXT_TOGGLE" = "xyes")

dnl for i386 --------------------------------------------------------------------------
AC_ARG_ENABLE(i386, AC_HELP_STRING([--enable-i386], [i386 build]),
	[
	 case "${enableval}" in
	  yes) IS_I386=yes ;;
	  no)  IS_I386=no ;;
	  *)   AC_MSG_ERROR(bad value ${enableval} for --enable-i386) ;;
	 esac
	],
	[IS_I386=no])
AM_CONDITIONAL([IS_I386], [test "x$IS_I386" = "xyes"])

dnl use ext-pdpushsrc --------------------------------------------------------------------------
AC_ARG_ENABLE(ext-pdpushsrc, AC_HELP_STRING([--enable-ext-pdpushsrc], [using pdpushsrc]),
[
 case "${enableval}" in
	 yes) GST_EXT_USE_EXT_PD_PUSHSRC=yes ;;
	 no)  GST_EXT_USE_EXT_PD_PUSHSRC=no ;;
	 *)   AC_MSG_ERROR(bad value ${enableval} for --enable-ext-pdpushsrc) ;;
 esac
 ],
 [GST_EXT_USE_EXT_PD_PUSHSRC=yes])
AM_CONDITIONAL(GST_EXT_USE_EXT_PD_PUSHSRC, test "x$GST_EXT_USE_EXT_PD_PUSHSRC" = "xyes")
dnl use ext-audiotp --------------------------------------------------------------------------
AC_ARG_ENABLE(ext-audiotp, AC_HELP_STRING([--enable-ext-audiotp], [using audiotp]),
[
 case "${enableval}" in
	 yes) GST_EXT_USE_EXT_AUDIOTP=yes ;;
	 no)  GST_EXT_USE_EXT_AUDIOTP=no ;;
	 *)   AC_MSG_ERROR(bad value ${enableval} for --enable-ext-audiotp) ;;
 esac
 ],
 [GST_EXT_USE_EXT_AUDIOTP=yes])
AM_CONDITIONAL(GST_EXT_USE_EXT_AUDIOTP, test "x$GST_EXT_USE_EXT_AUDIOTP" = "xyes")

dnl use ext-audioeq --------------------------------------------------------------------------
AC_ARG_ENABLE(ext-audioeq, AC_HELP_STRING([--enable-ext-audioeq], [using audioeq]),
[
 case "${enableval}" in
         yes) GST_EXT_USE_EXT_AUDIOEQ=yes ;;
         no)  GST_EXT_USE_EXT_AUDIOEQ=no ;;
         *)   AC_MSG_ERROR(bad value ${enableval} for --enable-ext-audioeq) ;;
 esac
 ],
 [GST_EXT_USE_EXT_AUDIOEQ=yes])
AM_CONDITIONAL(GST_EXT_USE_EXT_AUDIOEQ, test "x$GST_EXT_USE_EXT_AUDIOEQ" = "xyes")

dnl use ext-piffdemux --------------------------------------------------------------------------
AC_ARG_ENABLE(ext-piffdemux, AC_HELP_STRING([--enable-ext-piffdemux], [using piffdemux]),
[
 case "${enableval}" in
	 yes) GST_EXT_USE_EXT_PIFFDEMUX=yes ;;
	 no)  GST_EXT_USE_EXT_PIFFDEMUX=no ;;
	 *)   AC_MSG_ERROR(bad value ${enableval} for --enable-ext-piffdemux) ;;
 esac
 ],
 [GST_EXT_USE_EXT_PIFFDEMUX=yes])
AM_CONDITIONAL(GST_EXT_USE_EXT_PIFFDEMUX, test "x$GST_EXT_USE_EXT_PIFFDEMUX" = "xyes")

#if test "x$GST_EXT_USE_EXT_PIFFDEMUX" = "xyes"; then
#	PKG_CHECK_MODULES(DRM_CLIENT, drm-client)
#	AC_SUBST(DRM_CLIENT_CFLAGS)
#	AC_SUBST(DRM_CLIENT_LIBS)
#	PKG_CHECK_MODULES(DRM_TRUSTED, drm-trusted)
#	AC_SUBST(DRM_TRUSTED_CFLAGS)
#	AC_SUBST(DRM_TRUSTED_LIBS)
#fi

dnl use ext-ssdemux --------------------------------------------------------------------------
AC_ARG_ENABLE(ext-ssdemux, AC_HELP_STRING([--enable-ext-ssdemux], [using ssdemux]),
[
 case "${enableval}" in
	 yes) GST_EXT_USE_EXT_SSDEMUX=yes ;;
	 no)  GST_EXT_USE_EXT_SSDEMUX=no ;;
	 *)   AC_MSG_ERROR(bad value ${enableval} for --enable-ext-ssdemux) ;;
 esac
 ],
 [GST_EXT_USE_EXT_SSDEMUX=yes])
AM_CONDITIONAL(GST_EXT_USE_EXT_SSDEMUX, test "x$GST_EXT_USE_EXT_SSDEMUX" = "xyes")

dnl use ext-dashdemux -----------------------------------------------------------------------
AC_ARG_ENABLE(ext-dashdemux, AC_HELP_STRING([--enable-ext-dashdemux], [using dashdemux]),
  [
    case "${enableval}" in
      yes) GST_EXT_USE_EXT_DASHDEMUX=yes ;;
      no)  GST_EXT_USE_EXT_DASHDEMUX=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-ext-dashdemux) ;;
    esac
  ],
  [GST_EXT_USE_EXT_DASHDEMUX=yes])
AM_CONDITIONAL(GST_EXT_USE_EXT_DASHDEMUX, test "x$GST_EXT_USE_EXT_DASHDEMUX" = "xyes")

dnl use ext-hlsdemux2 -----------------------------------------------------------------------
AC_ARG_ENABLE(ext-hlsdemux2, AC_HELP_STRING([--enable-ext-hlsdemux2], [using hlsdemux2]),
  [
    case "${enableval}" in
      yes) GST_EXT_USE_EXT_HLSDEMUX2=yes ;;
      no)  GST_EXT_USE_EXT_HLSDEMUX2=no ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-ext-hlsdemux2) ;;
    esac
  ]
  [GST_EXT_USE_EXT_HLSDEMUX2=yes])
AM_CONDITIONAL(GST_EXT_USE_EXT_HLSDEMUX2, test "x$GST_EXT_USE_EXT_HLSDEMUX2" = "xyes")

if test "x$GST_EXT_USE_EXT_HLSDEMUX2" = "xyes"; then                                                                       
    dnl crypto for hlsdemux                                                                                                 
    PKG_CHECK_MODULES(CRYPTO, libcrypto)                                                                                    
    AC_SUBST(CRYPTO_CFLAGS)                                                                                                 
    AC_SUBST(CRYPTO_LIBS)  
fi

AC_OUTPUT(
Makefile
common/Makefile
common/m4/Makefile
pdpushsrc/Makefile
pdpushsrc/src/Makefile
encodebin/Makefile
encodebin/src/Makefile
evasimagesink/Makefile
evasimagesink/src/Makefile
evaspixmapsink/Makefile
xvimagesrc/Makefile
xvimagesrc/src/Makefile
toggle/Makefile
toggle/src/Makefile
drmsrc/Makefile
drmsrc/src/Makefile
submux/Makefile
submux/src/Makefile
audiotp/Makefile
audiotp/src/Makefile
audioeq/Makefile
audioeq/src/Makefile
piffdemux/Makefile
piffdemux/src/Makefile
ssdemux/Makefile
ssdemux/src/Makefile
dashdemux/Makefile
dashdemux/src/Makefile
hlsdemux2/Makefile
hlsdemux2/src/Makefile
)
