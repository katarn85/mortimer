plugin_LTLIBRARIES = libgstspdifsink.la

libgstspdifsink_la_SOURCES =  spdifsink.c \
			      spdif_compress.c
libgstspdifsink_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstspdifsink_la_LIBADD = \
	$(GST_BASE_LIBS) \
	$(GST_LIBS) \
	$(top_builddir)/gst-libs/gst/audio/libgstaudio-$(GST_MAJORMINOR).la

libgstspdifsink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstspdifsink_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = spdifsink.h \
		spdif_compress_params.h \
		spdif_tinycompress.h \
		spdif_compress_offload.h
