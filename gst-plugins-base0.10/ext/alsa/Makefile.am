plugin_LTLIBRARIES = libgstalsa.la

libgstalsa_la_SOURCES = \
	gstalsadeviceprobe.c \
	gstalsamixer.c	\
	gstalsamixerelement.c \
	gstalsamixertrack.c \
	gstalsamixeroptions.c \
	gstalsaplugin.c \
	gstalsasink.c 	\
	gstalsasrc.c \
	gstalsa.c \
	amixer.c

libgstalsa_la_CFLAGS = \
	$(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) \
	$(GST_CFLAGS) \
	$(ALSA_CFLAGS)\
	$(AVOC_CFLAGS) \
	$(AUDIOSESSIONMGR_CFLAGS) \
	$(SECAUDIO_CFLAGS)

libgstalsa_la_LIBADD = \
	$(top_builddir)/gst-libs/gst/interfaces/libgstinterfaces-$(GST_MAJORMINOR).la 	\
	$(top_builddir)/gst-libs/gst/audio/libgstaudio-$(GST_MAJORMINOR).la \
	$(GST_BASE_LIBS) \
	$(ALSA_LIBS) \
	$(AVOC_LIBS) \
	$(SECAUDIO_LIBS)

libgstalsa_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstalsa_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = \
	gstalsa.h \
	gstalsadeviceprobe.h \
	gstalsamixer.h \
	gstalsamixerelement.h \
	gstalsamixertrack.h \
	gstalsamixeroptions.h \
	gstalsasrc.h \
	gstalsasink.h \
	amixer.h
