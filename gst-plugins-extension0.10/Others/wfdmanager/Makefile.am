SUBDIRS = extension

plugin_LTLIBRARIES = libgstwfdmanager.la

libgstwfdmanager_la_SOURCES = gstwfdmanager.c \
                 gstwfdrtspsrc.c \
                 gstwfdrtprequester.c \
                 gstwfdrtspext.c \
                 wfdrtpbuffer/wfdrtpbuffer.c \
                 wfdrtpbuffer/gstwfdrtpbuffer.c \
                 wfdrtpbuffer/rtpstats.c

if ENABLE_WFD_EXTENDED_FEATURES
else
libgstwfdmanager_la_SOURCES += wfdrtspmanager.c
endif

noinst_HEADERS = gstwfdrtspsrc.h \
                 gstwfdrtprequester.h \
                 gstwfdrtspext.h \
                 wfdrtpbuffer/wfdrtpbuffer.h \
                 wfdrtpbuffer/gstwfdrtpbuffer.h \
                 wfdrtpbuffer/rtpstats.h \
                 wfdrtpbuffer/glib-compat-private.h

if ENABLE_WFD_EXTENDED_FEATURES
else
noinst_HEADERS += wfdrtspmanager.h
endif

libgstwfdmanager_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS)

if ENABLE_WFD_EXTENDED_FEATURES
libgstwfdmanager_la_CFLAGS += -DENABLE_WFD_EXTENDED_FEATURES
endif

if ENABLE_WFD_SINK_UIBC
libgstwfdmanager_la_CFLAGS += -DENABLE_WFD_SINK_UIBC
endif

libgstwfdmanager_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) \
               -lgstinterfaces-@GST_MAJORMINOR@ \
               -lgstrtp-@GST_MAJORMINOR@ -lgstrtsp-@GST_MAJORMINOR@ \
               $(WIN32_LIBS)


libgstwfdmanager_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstwfdmanager_la_LIBTOOLFLAGS = --tag=disable-static
