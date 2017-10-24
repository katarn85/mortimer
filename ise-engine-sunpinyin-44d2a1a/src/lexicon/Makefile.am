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
INCLUDES=-I$(top_srcdir)\
         -I$(top_srcdir)/src\
         -I$(top_srcdir)/src/ime-core\
         -I$(top_srcdir)/src/lexicon\
         -I$(top_srcdir)/src/pinyin\
         -I$(top_srcdir)/src/slm
         
noinst_HEADERS = pytrie.h\
                     pytrie_gen.h\
                     trie_writer.h

noinst_LTLIBRARIES = liblexicon.la
liblexicon_la_SOURCES 		= pytrie.cpp
