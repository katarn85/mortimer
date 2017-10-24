plugin_LTLIBRARIES = libgstremotesink.la

libgstremotesink_la_SOURCES = \
	gstremotesink.c \
	gstremoteaudiosink.c \
	gstremotesinkplugin.c

libgstremotesink_la_CFLAGS = \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS)

libgstremotesink_la_LIBADD = \
	$(top_builddir)/gst-libs/gst/interfaces/libgstinterfaces-$(GST_MAJORMINOR).la \
	$(GST_BASE_LIBS)

libgstremotesink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstremotesink_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
	gstremotesink.h \
	gstremoteaudiosink.h