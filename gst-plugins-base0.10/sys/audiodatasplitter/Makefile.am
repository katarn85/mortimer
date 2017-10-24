plugin_LTLIBRARIES = libgstaudiodatasplitter.la

libgstaudiodatasplitter_la_SOURCES =  audiodatasplitter.c
libgstaudiodatasplitter_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstaudiodatasplitter_la_LIBADD = \
	$(GST_BASE_LIBS) \
	$(GST_LIBS)
libgstaudiodatasplitter_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstaudiodatasplitter_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = audiodatasplitter.h
