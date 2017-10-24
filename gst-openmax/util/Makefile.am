noinst_LTLIBRARIES = libutil.la

utilincludedir = ${includedir}/gstomx
utilinclude_HEADERS = *.h

libutil_la_SOURCES = async_queue.c async_queue.h \
		     sem.c sem.h

libutil_la_CFLAGS = $(GTHREAD_CFLAGS)
libutil_la_LIBADD = $(GTHREAD_LIBS)
