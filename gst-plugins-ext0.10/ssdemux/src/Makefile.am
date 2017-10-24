# plugindir is set in configure

##############################################################################
# change libgstplugin.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libgstssdemux.la

##############################################################################
# for the next set of variables, rename the prefix if you renamed the .la,   #
#  e.g. libgstplugin_la_SOURCES => libmysomething_la_SOURCES                 #
#       libgstplugin_la_CFLAGS  => libmysomething_la_CFLAGS                  #
#       libgstplugin_la_LIBADD  => libmysomething_la_LIBADD                  #
#       libgstplugin_la_LDFLAGS => libmysomething_la_LDFLAGS                 #
##############################################################################

# sources used to compile this plug-in
libgstssdemux_la_SOURCES = gstssdemux.c ssmanifestparse.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstssdemux_la_CFLAGS = $(GST_CFLAGS) $(GST_BASE_CFLAGS) $(SOUP_CFLAGS) $(XML2_CFLAGS) $(GST_APP_CFLAGS) -I$(srcdir)/../../piffdemux/src
libgstssdemux_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(SOUP_LIBS) $(XML2_LIBS) $(GST_APP_LIBS) -lgstapp-0.10
libgstssdemux_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gstssdemux.h ssmanifestparse.h
