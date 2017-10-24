aclocal
libtoolize --copy -f
autoheader
autoconf
automake --add-missing --copy --foreign
#./configure --with-xo-machine=W1
