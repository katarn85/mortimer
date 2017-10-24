plugin_LTLIBRARIES = libgstframecollector.la

libgstframecollector_la_SOURCES =  framecollector.c 
libgstframecollector_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstframecollector_la_LIBADD = \
	$(GST_BASE_LIBS) \
	$(GST_LIBS)
libgstframecollector_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstframecollector_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = framecollector.h
