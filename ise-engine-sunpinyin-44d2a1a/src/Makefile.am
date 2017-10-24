## Makefile.am -- Process this file with automake to produce Makefile.in
## Copyright (C) 2002 James Su
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


SUBDIRS 		= ime-core lexicon pinyin slm
MAINTAINERCLEANFILES 	= Makefile.in

#####################portability##########################
noinst_LTLIBRARIES = libportability.la
libportability_la_SOURCES 		= portability.cpp

libsunpinyinincludedir       = $(includedir)/sunpinyin-2.0
libsunpinyininclude_HEADERS 	= portability.h\
                                    sunpinyin.h\
                                    ime-core/imi_option_event.h\
                                    ime-core/imi_view.h\
                                    ime-core/imi_options.h\
                                    ime-core/imi_keys.h\
                                    ime-core/imi_option_keys.h\
				    ime-core/imi_winHandler.h\
				    ime-core/imi_uiobjects.h
#####################sunpinyin##########################
SUNPINYIN_DEFINES=-DSUNPINYIN_DATA_DIR=\"@SUNPINYIN_DATA_DIR@\"
moduledir = /usr/lib
module_LTLIBRARIES = libsunpinyin.la

libsunpinyin_la_SOURCES = 

libsunpinyin_la_CXXFLAGS = @ISF_CXXFLAGS@ @SQLITE_CXXFLAGS@ @GLIB_CXXFLAGS@ $(SUNPINYIN_DEFINES)

libsunpinyin_la_LDFLAGS = -avoid-version \
					-module \
					-export-dynamic\
					@ISF_LIBS@ \
					@SQLITE_LIBS@\
					@GLIB_LIBS@\
					-lstdc++ 

libsunpinyin_la_LIBADD = libportability.la\
                        $(top_srcdir)/src/ime-core/libime-core.la\
                        $(top_srcdir)/src/lexicon/liblexicon.la\
                        $(top_srcdir)/src/pinyin/libpinyin.la\
                        $(top_srcdir)/src/slm/libslm.la
