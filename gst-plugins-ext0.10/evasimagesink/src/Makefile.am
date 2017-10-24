# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstevasimagesink.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstevasimagesink.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

# sources used to compile this plug-in
libgstevasimagesink_la_SOURCES = gstevasimagesink.c gstevasimagesink.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstevasimagesink_la_CFLAGS = $(GST_CFLAGS) $(GST_VIDEO_CFLAGS) $(EFL_CFLAGS) $(NATIVE_BUFFER_CFLAGS) $(TBM_CFLAGS)
libgstevasimagesink_la_LIBADD = $(GST_LIBS) $(GST_VIDEO_LIBS) $(EFL_LIBS) $(NATIVE_BUFFER_LIBS) $(TBM_LIBS)
libgstevasimagesink_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstevasimagesink_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstevasimagesink.h
