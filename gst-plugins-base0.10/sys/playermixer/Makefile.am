plugin_LTLIBRARIES = libgstplayermixer.la

includelibgstplayermixerdir = $(includedir)/gstreamer-0.10/sys/playermixer

includelibgstplayermixer_HEADERS = \
	player_mixer.h

libgstplayermixer_la_SOURCES =  \
	player_mixer.c\
	tbm_utils.c

libgstplayermixer_la_CFLAGS = \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	$(DLOG_CFLAGS) \
	$(AUDIOSESSIONMGR_CFLAGS) \
	$(DRI2PROTO_CFLAGS) \
	$(DRI2_CFLAGS) \
	$(TBM_CFLAGS)

libgstplayermixer_la_LIBADD = \
	$(top_builddir)/gst-libs/gst/interfaces/libgstinterfaces-$(GST_MAJORMINOR).la \
	$(top_builddir)/gst-libs/gst/app/libgstapp-$(GST_MAJORMINOR).la \
	$(GST_BASE_LIBS) \
	$(GST_LIBS) \
        $(DLOG_LIBS) \
	$(DRI2PROTO_LIBS) \
        $(DRI2_LIBS) \
        $(X_LIBS) $(XVIDEO_LIBS) $(XSHM_LIBS) $(LIBM)\
        $(TBM_LIBS) $(XDAMAGE_LIBS)

libgstplayermixer_la_LIBTOOLFLAGS = --tag=disable-static
noinst_HEADERS = player_mixer_log.h tbm_utils.h
