# plugindir is set in configure

plugin_LTLIBRARIES = libgstcamerasrc.la

# sources used to compile this plug-in
libgstcamerasrc_la_SOURCES = gstcamerasrccontrol.c \
                             gstcamerasrccolorbalance.c \
                             camerasrc.c \
                             camerasrc-internal.c \
                             gstcamerasrc.c

libgstcamerasrc_la_CFLAGS = $(GST_CFLAGS) \
                            $(GST_BASE_CFLAGS) \
                            $(GST_VIDEO_FLAGS) \
                            $(MMTA_CFLAGS) \
                            -I$(srcdir)/include \
                            $(GST_INTERFACES_CFLAGS)


libgstcamerasrc_la_LIBADD = $(GST_LIBS) \
                            $(GST_BASE_LIBS) \
                            $(GST_VIDEO_LIBS) \
                            $(MMTA_LIBS) \
                            -ldl -lpthread -lrt -lm \
                            $(GST_INTERFACES_LIBS)

libgstcamerasrc_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)

libgstcamerasrc_la_CPPFLAGS = -I$(srcdir)/include

