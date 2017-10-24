#!/bin/sh
libtoolize --force --copy --automake
aclocal
autoheader
automake --foreign --copy --add-missing
autoconf
					
