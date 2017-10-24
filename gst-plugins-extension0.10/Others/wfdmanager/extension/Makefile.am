if ENABLE_WFD_EXTENDED_FEATURES
lib_LTLIBRARIES = libwfdrtspmanagerext.la

libwfdrtspmanagerext_la_SOURCES = wfdrtspmanagerext.c

libwfdrtspmanagerext_la_CFLAGS =  -I$(srcdir) $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS)
libwfdrtspmanagerext_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) \
               -lgstrtp-@GST_MAJORMINOR@ -lgstrtsp-@GST_MAJORMINOR@

noinst_HEADERS = wfdrtspmanagerext.h
endif

