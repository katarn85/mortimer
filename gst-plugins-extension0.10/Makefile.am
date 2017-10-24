# Copyright (c) 2006 Software Laboratory, SAMSUNG Electronics, Inc.
# All rights reserved.
#
# This software is the confidential and proprietary information of
# SAMSUNG Electronics, Inc. ("Confidential Information").
# You shall not disclose such Confidential Information and shall use it
# only in accordance with the terms of the license agreement you entered into
# with SAMSUNG Electronics.
#
# Orignal by Jeechul jeon <heechul.jeon@samsung.com>


ACLOCAL_AMFLAGS = -I common/m4

aclocaldir = $(datadir)/aclocal

SUBDIRS = \
	common \
	Others

DIST_SUBDIRS = \
	common \
	Others

EXTRA_DIST = \
	gstreamer.spec gstreamer.spec.in \
	configure.ac autogen.sh depcomp \
	MAINTAINERS ABOUT-NLS RELEASE gstreamer.doap \
	$(win32)

check: check-exports

