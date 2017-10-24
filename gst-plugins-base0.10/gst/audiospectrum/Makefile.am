noinst_HEADERS = gstaudiospectrum.h

plugin_LTLIBRARIES = libgstaudiospectrum.la

libgstaudiospectrum_la_SOURCES = \
	gstaudiospectrum.c \
	utils/spectrum.c \
	utils/audiotools.c \
	utils/kiss_fft.c \
	utils/windowfft.c

libgstaudiospectrum_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS)
libgstaudiospectrum_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstaudiospectrum_la_LIBADD = $(GST_BASE_LIBS) $(GST_LIBS)
libgstaudiospectrum_la_LIBTOOLFLAGS = --tag=disable-static

