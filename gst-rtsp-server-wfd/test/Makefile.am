bin_PROGRAMS = mm_rtsp_testsuite

mm_rtsp_testsuite_SOURCES = mm_rtsp_testsuite.c

mm_rtsp_testsuite_CFLAGS = -I$(srcdir)/../src/include \
				$(GLIB_CFLAGS)\
				$(GST_CFLAGS) \
				$(GST_INTERFACE_CFLAGS) \
				$(GST_APP_CFLAGS) \
				$(GST_PLUGINS_BASE_CFLAGS)\
				$(INIPARSER_CFLAGS) \
				$(MMCOMMON_CFLAGS) \
				$(VCONF_CFLAGS)\
				$(UTILX_CFLAGS)\
				$(X11_CFLAGS)

############################################

mm_rtsp_testsuite_LDADD =  ../src/libmmfwfd_rtsp_server.la\
				$(GLIB_LIBS)\
				$(GST_LIBS) \
				$(GST_INTERFACE_LIBS) \
				$(GST_BASE_LIBS) \
				$(GST_APP_LIBS) \
				$(GST_PLUGINS_BASE_LIBS)\
				$(MMCOMMON_LIBS) \
				$(MM_WFD_COMMON_LIBS) \
				$(INIPARSER_LIBS)\
				$(VCONF_LIBS)\
				$(UTILX_LIBS)\
				$(X11_LIBS)
