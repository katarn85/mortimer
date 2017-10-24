
plugin_LTLIBRARIES = libgstpiff.la

libgstpiffincludedir = $(includedir)/gstreamer-@GST_MAJORMINOR@/gst/

libgstpiff_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) $(DRM_CLIENT_CFLAGS) $(DRM_TRUSTED_CFLAGS)

libgstpiff_la_LIBADD = \
    $(GST_PLUGINS_BASE_LIBS) \
    -lgstriff-@GST_MAJORMINOR@ \
    -lgstaudio-@GST_MAJORMINOR@ \
    -lgsttag-@GST_MAJORMINOR@ \
    -lgstpbutils-@GST_MAJORMINOR@ \
    $(GST_BASE_LIBS) $(GST_LIBS) $(ZLIB_LIBS) $(DRM_CLIENT_LIBS) $(DRM_TRUSTED_LIBS)

libgstpiff_la_LDFLAGS = ${GST_PLUGIN_LDFLAGS}

libgstpiff_la_SOURCES = piff-plugin.c \
		piffdemux.c \
		piffdemux_types.c \
		piffdemux_dump.c

libgstpiff_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
	piffatomparser.h   \
	piffdemux_types.h  \
	piffdemux_dump.h   \
	piffdemux_fourcc.h \
	piffpalette.h \
    piffcommon.h

#libgstpiffinclude_HEADERS = piffcommon.h

Android.mk: Makefile.am $(BUILT_SOURCES)
	androgenizer \
	-:PROJECT libgstpiff -:SHARED libgstpiff \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgstpiff_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(DEFAULT_INCLUDES) $(CPPFLAGS) $(libgstpiff_la_CFLAGS) \
	 -:LDFLAGS $(libgstpiff_la_LDFLAGS) \
	           $(libgstpiff_la_LIBADD) \
	           -ldl \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
		       LOCAL_MODULE_PATH:='$$(TARGET_OUT)/lib/gstreamer-0.10' \
	> $@
