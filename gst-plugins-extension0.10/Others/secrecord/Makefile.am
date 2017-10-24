# plugindir is set in configure

##############################################################################
# change libgstplugin.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libgstsecrecord.la

##############################################################################
# for the next set of variables, rename the prefix if you renamed the .la,   #
#  e.g. libgstplugin_la_SOURCES => libmysomething_la_SOURCES                 #
#       libgstplugin_la_CFLAGS  => libmysomething_la_CFLAGS                  #
#       libgstplugin_la_LIBADD  => libmysomething_la_LIBADD                  #
#       libgstplugin_la_LDFLAGS => libmysomething_la_LDFLAGS                 #
##############################################################################

# sources used to compile this plug-in
libgstsecrecord_la_SOURCES = gstsecrecord.c

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstsecrecord_la_CFLAGS = $(GST_CFLAGS) \
			    $(GST_BASE_CFLAGS) \
			    $(INIPARSER_CFLAGS) \
			    $(VCONF_CFLAGS) \
			    $(AUDIOFILTERS_SEC_REC_CFLAGS) \
			    $(AUDIOFILTERS_SEC_NRSS_CFLAGS)

libgstsecrecord_la_LIBADD = $(GST_LIBS) \
			    $(GST_BASE_LIBS) \
			    $(INIPARSER_LIBS) \
			    $(VCONF_LIBS) \
			    $(AUDIOFILTERS_SEC_REC_LIBS) \
			    $(AUDIOFILTERS_SEC_NRSS_LIBS)

libgstsecrecord_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gstsecrecord.h

