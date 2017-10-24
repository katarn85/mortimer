# plugindir is set in configure

##############################################################################
# change libgstplugin.la to something more suitable, e.g. libmysomething.la  #
##############################################################################
plugin_LTLIBRARIES = libgstsoundalive.la

##############################################################################
# for the next set of variables, rename the prefix if you renamed the .la,   #
#  e.g. libgstplugin_la_SOURCES => libmysomething_la_SOURCES                 #
#       libgstplugin_la_CFLAGS  => libmysomething_la_CFLAGS                  #
#       libgstplugin_la_LIBADD  => libmysomething_la_LIBADD                  #
#       libgstplugin_la_LDFLAGS => libmysomething_la_LDFLAGS                 #
##############################################################################

# sources used to compile this plug-in
libgstsoundalive_la_SOURCES = gstsoundalive.c

if !GST_EXT_SOUNDALIVE_DISABLE_SA
libgstsoundalive_la_SOURCES += soundalive_layer_wrapper.cpp
endif

# flags used to compile this plugin
# add other _CFLAGS and _LIBS as needed
libgstsoundalive_la_CFLAGS = $(GST_CFLAGS) \
			     $(GST_BASE_CFLAGS) \
			     $(MM_SOUND_CFLAGS) \
			     $(AUDIOFILTERS_SEC_SB_CFLAGS)
if !GST_EXT_SOUNDALIVE_DISABLE_SA
libgstsoundalive_la_CFLAGS += $(AUDIOFILTERS_SEC_SA_CFLAGS) \
                              $(AUDIOFILTERS_SEC_SRC_CFLAGS)
endif

libgstsoundalive_la_LIBADD = $(GST_LIBS) \
			     $(GST_BASE_LIBS) \
			     $(INIPARSER_LIBS) \
			     $(MM_SOUND_LIBS) \
			     $(AUDIOFILTERS_SEC_SB_LIBS)

if !GST_EXT_SOUNDALIVE_DISABLE_SA
libgstsoundalive_la_LIBADD += $(AUDIOFILTERS_SEC_SA_LIBS) \
                              $(AUDIOFILTERS_SEC_SRC_LIBS)
endif

libgstsoundalive_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

# headers we need but don't want installed
noinst_HEADERS = gstsoundalive.h

