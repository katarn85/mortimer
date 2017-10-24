plugin_LTLIBRARIES = libgstwfdtsdemux.la

 libgstwfdtsdemux_la_SOURCES = \
	wfdtspatinfo.c \
	wfdtspmtinfo.c \
	wfdtspmtstreaminfo.c \
	gstmpegdesc.c \
	gstwfdtsdemux.c \
	gstpesfilter.c\
	gstsectionfilter.c

 libgstwfdtsdemux_la_CFLAGS = \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) $(GST_CFLAGS) -ldrmspecificinfo
 libgstwfdtsdemux_la_LIBADD = \
	$(GST_PLUGINS_BASE_LIBS) -lgsttag-$(GST_MAJORMINOR) \
	$(GST_BASE_LIBS) $(GST_LIBS) -ldrmspecificinfo
 libgstwfdtsdemux_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
 libgstwfdtsdemux_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
	gstmpegdefs.h   \
	gstmpegdesc.h   \
	gstpesfilter.h  \
	gstwfdtsdemux.h \
	wfdtspatinfo.h \
	wfdtspmtinfo.h \
	wfdtspmtstreaminfo.h \
	gstsectionfilter.h
