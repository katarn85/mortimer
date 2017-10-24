# plugindir is set in configure

##############################################################################
# change libgstplugin.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libgstpdpushsrc.la

##############################################################################
# for the next set of variables, rename the prefix if you renamed the .la,   #
#  e.g. libgstplugin_la_SOURCES => libmysomething_la_SOURCES                 #
#       libgstplugin_la_CFLAGS  => libmysomething_la_CFLAGS                  #
#       libgstplugin_la_LIBADD  => libmysomething_la_LIBADD                  #
#       libgstplugin_la_LDFLAGS => libmysomething_la_LDFLAGS                 #
##############################################################################

# sources used to compile this plug-in
libgstpdpushsrc_la_SOURCES = gstpdpushsrc.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstpdpushsrc_la_CFLAGS = $(GST_CFLAGS)  $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS)
libgstpdpushsrc_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS)  $(GST_BASE_LIBS)
libgstpdpushsrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gstpdpushsrc.h
