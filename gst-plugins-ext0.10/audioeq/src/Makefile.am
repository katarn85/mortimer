# plugindir is set in configure

##############################################################################
# change libgstplugin.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libgstaudioeq.la

##############################################################################
# for the next set of variables, rename the prefix if you renamed the .la,   #
#  e.g. libgstplugin_la_SOURCES => libmysomething_la_SOURCES                 #
#       libgstplugin_la_CFLAGS  => libmysomething_la_CFLAGS                  #
#       libgstplugin_la_LIBADD  => libmysomething_la_LIBADD                  #
#       libgstplugin_la_LDFLAGS => libmysomething_la_LDFLAGS                 #
##############################################################################

# sources used to compile this plug-in
libgstaudioeq_la_SOURCES = gstaudioeq.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstaudioeq_la_CFLAGS = $(GST_CFLAGS)  $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CONTROLLER_CFLAGS)
libgstaudioeq_la_LIBADD = $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS) -lgstaudio-$(GST_MAJORMINOR) $(GST_BASE_LIBS) $(GST_CONTROLLER_LIBS) $(GST_LIBS) $(LIBM) -lgstbase-0.10  -lglib-2.0 -lgstcontroller-0.10
libgstaudioeq_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

libgstequalizer_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstaudioeq.h
