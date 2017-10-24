plugin_LTLIBRARIES = libgstxvimagesink.la

libgstxvimagesink_la_SOURCES =  \
	xvimagesink.c \
	videoframerotate.c \
	xv_pixmap_utils.c \
	nv12_rgb.S \
	nv21_rgb.S

libgstxvimagesink_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) $(X_CFLAGS) $(XFIXES_CFLAGS) $(DRI2PROTO_CFLAGS) $(DRI2_CFLAGS) $(TBM_CFLAGS) $(XDAMAGE_CFLAGS) $(AUDIOSESSIONMGR_CFLAGS) $(AVOC_CFLAGS) $(RESLOOKUP_CFLAGS)
libgstxvimagesink_la_LIBADD = \
	$(top_builddir)/gst-libs/gst/interfaces/libgstinterfaces-$(GST_MAJORMINOR).la \
	$(top_builddir)/gst-libs/gst/video/libgstvideo-$(GST_MAJORMINOR).la \
	$(GST_BASE_LIBS) \
	$(GST_LIBS) \
	$(XFIXES_LIBS) \
	$(DRI2PROTO_LIBS) \
	$(DRI2_LIBS) \
	$(X_LIBS) $(XVIDEO_LIBS) $(XSHM_LIBS) $(LIBM) \
	$(TBM_LIBS) $(XDAMAGE_LIBS) $(AVOC_LIBS) $(RESLOOKUP_LIBS)
libgstxvimagesink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstxvimagesink_la_DEPENDENCIES = $(top_builddir)/gst-libs/gst/video/libgstvideo-$(GST_MAJORMINOR).la
libgstxvimagesink_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
	xvimagesink.h \
	videoframerotate.h \
	xv_pixmap_utils.h
