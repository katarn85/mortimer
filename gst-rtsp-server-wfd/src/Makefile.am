lib_LTLIBRARIES = libmmfwfd_rtsp_wfd.la

includelibmmfwfd_rtsp_wfddir = $(includedir)/mmf

includelibmmfwfd_rtsp_wfd_HEADERS = wfd/include/rtsp-server-wfd.h

libmmfwfd_rtsp_wfd_la_SOURCES = wfd/rtsp-server-wfd.c wfd/rtsp-client-wfd.c

libmmfwfd_rtsp_wfd_la_CFLAGS = -Werror -I$(srcdir)/include \
        -I$(srcdir)/wfd/include \
        -I$(srcdir)/extended/include \
        $(GLIB_CFLAGS) \
        $(GST_CFLAGS) \
        $(GST_PLUGINS_BASE_CFLAGS) \
        $(GST_INTERFACE_CFLAGS) \
        $(GST_APP_CFLAGS) \
        $(X11_CFLAGS)\
        $(XAU_CFLAGS)\
        $(XDMCP_CFLAGS)\
        $(XEXT_CFLAGS)\
        $(LIBDRI2_CFLAGS)\
        $(LIBDRM_CFLAGS)\
        $(MM_WFD_COMMON_CFLAGS) \
        $(APPFW_APPLICATION_CFLAGS)

if ENABLE_WFD_EXTENDED_FEATURES
libmmfwfd_rtsp_wfd_la_CFLAGS += -DENABLE_WFD_EXTENDED_FEATURES
endif

if ENABLE_HDCP
libmmfwfd_rtsp_wfd_la_CFLAGS += $(HDCP_CFLAGS) -DUSE_HDCP
endif

if IS_QC_SPECIFIC
libmmfwfd_rtsp_wfd_la_CFLAGS += -DENABLE_QC_SPECIFIC
endif

libmmfwfd_rtsp_wfd_la_LIBADD = $(GLIB_LIBS) \
        -lgstrtp-0.10 -lgstrtsp-0.10 -lgstsdp-0.10 -lgstapp-0.10 \
        $(GST_LIBS) \
        $(MM_WFD_COMMON_LIBS) \
        $(MMCOMMON_LIBS)

if ENABLE_WFD_EXTENDED_FEATURES
lib_LTLIBRARIES += libmmfwfd_rtsp_server_ext.la

includelibmmfwfd_rtsp_server_extdir = $(includedir)/mmf

includelibmmfwfd_rtsp_server_ext_HEADERS = extended/include/rtsp-server-extended.h

libmmfwfd_rtsp_server_ext_la_SOURCES = extended/rtsp-server-extended.c extended/rtsp-client-extended.c

libmmfwfd_rtsp_server_ext_la_SOURCES +=	extended/rtsp-resender.c

libmmfwfd_rtsp_server_ext_la_CFLAGS = -Werror -I$(srcdir)/include \
        -I$(srcdir)/extended/include \
        $(GLIB_CFLAGS) \
        $(GST_CFLAGS) \
        $(GST_PLUGINS_BASE_CFLAGS) \
        $(GST_INTERFACE_CFLAGS) \
        $(GST_APP_CFLAGS) \
        $(APPFW_APPLICATION_CFLAGS) \
        $(VCONF_CFLAGS)

libmmfwfd_rtsp_server_ext_la_CFLAGS += -DENABLE_WFD_EXTENDED_FEATURES

libmmfwfd_rtsp_server_ext_la_LIBADD = $(GLIB_LIBS) \
        -lgstrtp-0.10 -lgstrtsp-0.10 -lgstsdp-0.10 -lgstapp-0.10 \
        -lmmfwfdconfig_ext \
        $(GST_LIBS) \
        $(MMCOMMON_LIBS) \
        $(APPFW_APPLICATION_LIBS)\
        $(VCONF_LIBS)

if ENABLE_HDCP
libmmfwfd_rtsp_server_ext_la_CFLAGS += -DUSE_HDCP
libmmfwfd_rtsp_server_ext_la_LIBADD += $(HDCP_LIBS)
endif

endif

lib_LTLIBRARIES += libmmfwfd_rtsp_server.la

includelibmmfwfd_rtsp_serverdir = $(includedir)/mmf

includelibmmfwfd_rtsp_server_HEADERS = include/rtsp-server.h \
	include/rtsp-auth.h \
	include/rtsp-server-common.h \
	include/rtsp-media-mapping.h \
	include/rtsp-client.h \
	include/rtsp-session-pool.h \
	include/rtsp-media.h \
	include/rtsp-media-factory.h \
	include/rtsp-session.h \
	extended/include/rtsp-server-extended.h 

libmmfwfd_rtsp_server_la_SOURCES = 	rtsp-funnel.c \
          rtsp-auth.c \
          rtsp-params.c \
          rtsp-sdp.c \
          rtsp-media.c \
          rtsp-media-factory.c \
          rtsp-media-factory-uri.c \
          rtsp-media-mapping.c \
          rtsp-session.c \
          rtsp-session-pool.c \
          rtsp-client.c \
          rtsp-server.c

libmmfwfd_rtsp_server_la_CFLAGS = -Werror -I$(srcdir)/include \
        -I$(srcdir)/wfd/include \
        -I$(srcdir)/extended/include \
        $(GLIB_CFLAGS) \
        $(GST_CFLAGS) \
        $(GST_INTERFACE_CFLAGS) \
        $(GST_APP_CFLAGS) \
        $(GST_PLUGINS_BASE_CFLAGS)\
        $(UTILX_CFLAGS) \
        $(MM_WFD_COMMON_CFLAGS) \
        $(APPFW_APPLICATION_CFLAGS)

if ENABLE_WFD_EXTENDED_FEATURES
libmmfwfd_rtsp_server_la_CFLAGS += -DENABLE_WFD_EXTENDED_FEATURES
libmmfwfd_rtsp_server_la_SOURCES += extended/include/rtsp-resender.h
endif

if IS_QC_SPECIFIC
libmmfwfd_rtsp_server_la_CFLAGS += -DENABLE_QC_SPECIFIC
endif

if ENABLE_HDCP
libmmfwfd_rtsp_server_la_CFLAGS += $(HDCP_CFLAGS) -DUSE_HDCP
endif

noinst_HEADERS = include/rtsp-funnel.h \
    include/rtsp-params.h \
    include/rtsp-sdp.h \
    include/rtsp-media-factory-uri.h    

#libmmfwfd_rtsp_server_la_DEPENDENCIES = $(top_builddir)/src/libmmfwfdconfig.la

libmmfwfd_rtsp_server_la_LIBADD = $(GLIB_LIBS) \
        $(GST_LIBS) \
        $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) \
        -lgstrtp-0.10 -lgstrtsp-0.10 -lgstsdp-0.10 -lgstapp-0.10 -lmmfwfd_rtsp_wfd\
        $(GST_INTERFACE_LIBS) \
        $(GST_APP_LIBS) \
        $(MM_WFD_COMMON_LIBS) \
        $(INIPARSER_LIBS)\
        $(X11_LIBS)\
        $(XAU_LIBS)\
        $(XDMCP_LIBS)\
        $(XEXT_LIBS)\
        $(LIBDRI2_LIBS)\
        $(LIBDRM_LIBS)\
        $(UTILX_LIBS)\
        $(APPFW_APPLICATION_LIBS)

