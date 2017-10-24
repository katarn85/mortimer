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


MAINTAINERCLEANFILES 	= Makefile.in
CLEANFILES                    = 	*.bak
INCLUDES=-I$(top_srcdir)\
         -I$(top_srcdir)/src\
         -I$(top_srcdir)/src/ime-core\
         -I$(top_srcdir)/src/lexicon\
         -I$(top_srcdir)/src/pinyin\
         -I$(top_srcdir)/src/slm
         
noinst_HEADERS = ic_history.h\
                     imi_context.h\
                     imi_data.h\
                     imi_defines.h\
                     imi_funcobjs.h\
                     imi_glibHandler.h\
                     imi_keys.h\
                     imi_option_event.h\
                     imi_option_keys.h\
                     imi_options.h\
                     imi_plugin.h\
                     imi_uiobjects.h\
                     imi_view_classic.h\
                     imi_view.h\
                     imi_winHandler.h\
                     lattice_states.h\
                     userdict.h\
                     utils.h

noinst_LTLIBRARIES = libime-core.la
libime_core_la_SOURCES 		=  imi_context.cpp\
           imi_data.cpp\
           lattice_states.cpp\
           imi_view.cpp\
           imi_uiobjects.cpp\
           imi_view_classic.cpp\
           imi_winHandler.cpp\
           ic_history.cpp\
           imi_funcobjs.cpp\
           imi_options.cpp\
           imi_option_event.cpp\
           userdict.cpp
libime_core_la_CXXFLAGS = -DSUNPINYIN_DATA_DIR=\"@SUNPINYIN_DATA_DIR@\"\
                         @SQLITE_CFLAGS@ @GLIB_CFLAGS@
