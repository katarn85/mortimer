# plugindir is set in configure

##############################################################################
# change libgstplugin.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libgstdrmsrc.la

##############################################################################
# for the next set of variables, rename the prefix if you renamed the .la,   #
#  e.g. libgstplugin_la_SOURCES => libmysomething_la_SOURCES                 #
#       libgstplugin_la_CFLAGS  => libmysomething_la_CFLAGS                  #
#       libgstplugin_la_LIBADD  => libmysomething_la_LIBADD                  #
#       libgstplugin_la_LDFLAGS => libmysomething_la_LDFLAGS                 #
##############################################################################

# sources used to compile this plug-in
libgstdrmsrc_la_SOURCES = gstdrmsrc.c drm_util.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstdrmsrc_la_CFLAGS = $(GST_CFLAGS) $(DRM_CLIENT_CFLAGS) $(DRM_TRUSTED_CFLAGS) -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
libgstdrmsrc_la_LIBADD = $(GST_LIBS) $(DRM_CLIENT_LIBS) $(DRM_TRUSTED_LIBS) $(GST_BASE_LIBS) 
libgstdrmsrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gstdrmsrc.h drm_util.h

