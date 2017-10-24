# plugindir is set in configure

##############################################################################
# change libgstplugin.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libgstxvimagesrc.la

##############################################################################
# for the next set of variables, rename the prefix if you renamed the .la,   #
#  e.g. libgstplugin_la_SOURCES => libmysomething_la_SOURCES                 #
#       libgstplugin_la_CFLAGS  => libmysomething_la_CFLAGS                  #
#       libgstplugin_la_LIBADD  => libmysomething_la_LIBADD                  #
#       libgstplugin_la_LDFLAGS => libmysomething_la_LDFLAGS                 #
##############################################################################

# sources used to compile this plug-in
libgstxvimagesrc_la_SOURCES = gstxvimagesrc.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstxvimagesrc_la_CFLAGS = $(GST_CFLAGS)  $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(TBM_CFLAGS) $(DRI2_CFLAGS) $(X11_CFLAGS) $(XEXT_CFLAGS) $(XV_CFLAGS) $(XDAMAGE_CFLAGS) $(DRM_DEVEL_CFLAGS) $(DRM_CFLAGS)
libgstxvimagesrc_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) $(GST_BASE_LIBS) $(TBM_LIBS) $(DRI2_LIBS) $(X11_LIBS) $(XEXT_LIBS) $(XV_LIBS) $(XDAMAGE_LIBS) $(DRM_LIBS) $(DRM_DEVEL_LIBS) $(DRM_SLP_MSM)
libgstxvimagesrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gstxvimagesrc.h
