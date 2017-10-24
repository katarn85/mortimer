glib_enum_define=GST_RECORD
glib_enum_prefix=gst_record

plugindir = $(libdir)/gstreamer-@GST_MAJORMINOR@

plugin_LTLIBRARIES = libgstencodebin.la 
libgstencodebin_la_SOURCES = gstencodebin.c
nodist_libgstencodebin_la_SOURCES = $(built_sources)
libgstencodebin_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS)
libgstencodebin_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstencodebin_la_LIBADD = \
	$(GST_LIBS)\
        $(GST_BASE_LIBS) \
        $(GST_LIBS)
if ISPROTECTOR_TARGET
libgstencodebin_la_CFLAGS += -DUSE_ENCODER_QUEUE_SET 
libgstencodebin_la_CFLAGS += -DVIDEO_ENC_QUE_TIME=4 -DAUDIO_ENC_QUE_TIME=4
endif

if ISVOLANS_TARGET
libgstencodebin_la_CFLAGS += -DUSE_ENCODER_QUEUE_SET 
libgstencodebin_la_CFLAGS += -DVIDEO_ENC_QUE_TIME=4 -DAUDIO_ENC_QUE_TIME=4
endif

BUILT_SOURCES = $(built_headers) $(built_sources)

CLEANFILES = $(BUILT_SOURCES)

include $(top_srcdir)/common/glib-gen.mak

