plugin_LTLIBRARIES = libgstmixersink.la

libgstmixersink_la_SOURCES =  \
	mixersink.c

libgstmixersink_la_CFLAGS = \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	$(DLOG_CFLAGS) \
	$(AUDIOSESSIONMGR_CFLAGS) \
	$(DRI2PROTO_CFLAGS) \
	$(DRI2_CFLAGS) \
	$(TBM_CFLAGS)

libgstmixersink_la_LIBADD = \
	$(top_builddir)/sys/playermixer/libgstplayermixer.la \
	$(GST_BASE_LIBS) \
	$(GST_LIBS) \
        $(DLOG_LIBS) \
	$(DRI2PROTO_LIBS) \
        $(DRI2_LIBS) \
        $(X_LIBS) $(XVIDEO_LIBS) $(XSHM_LIBS) $(LIBM)\
        $(TBM_LIBS) $(XDAMAGE_LIBS)

libgstmixersink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstmixersink_la_DEPENDENCIES = $(top_builddir)/sys/playermixer/libgstplayermixer.la
libgstmixersink_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = mixersink.h

