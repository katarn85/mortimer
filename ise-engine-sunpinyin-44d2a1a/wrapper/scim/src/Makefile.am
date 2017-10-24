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

MAINTAINERCLEANFILES	= Makefile.in

ise_engine_sunpinyin_DEFINES	= -DSCIM_ICONDIR=\"@SUNPINYIN_ICON_DIR@\"\
                -DSCIM_SUNPINYIN_LOCALEDIR=\"$(localedir)\"

INCLUDES = -I./ -I../../../ -I../../../src/ -I../../../src/ime-core
noinst_HEADERS	= imi_scimwin.h\
			sunpinyin_imengine.h\
			sunpinyin_imengine_config_keys.h\
			sunpinyin_keycode.h\
			sunpinyin_lookup_table.h\
			sunpinyin_private.h\
			sunpinyin_utils.h


moduledir = @SUNPINYIN_MODULE_DIR@/IMEngine
module_LTLIBRARIES = ise-engine-sunpinyin.la

ise_engine_sunpinyin_la_SOURCES =imi_scimwin.cpp\
                                  sunpinyin_imengine.cpp\
                                  sunpinyin_lookup_table.cpp\
                                  sunpinyin_utils.cpp

ise_engine_sunpinyin_la_CXXFLAGS = @ISF_CFLAGS@ @ECORE_CFLAGS@ ${ise_engine_sunpinyin_DEFINES}

ise_engine_sunpinyin_la_LDFLAGS = -avoid-version \
					-module \
					-export-dynamic\
					@ISF_LIBS@\
					@ECORE_LIBS@\
					 -lstdc++

ise_engine_sunpinyin_la_LIBADD = $(top_builddir)/src/libsunpinyin.la
