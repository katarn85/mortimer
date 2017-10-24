#!/bin/sh

### WARNING: DO NOT CHANGE CODES from HERE !!! ###
#import setup
cd `dirname $0`
_PWD=`pwd`
pushd ./ > /dev/null
while [ ! -f "./xo-setup.conf" ]
do
		cd ../
		SRCROOT=`pwd`
		if [ "$SRCROOT" == "/" ]; then
				echo "Cannot find xo-setup.conf !!"
				exit 1
		fi
done
popd > /dev/null
. ${SRCROOT}/xo-setup.conf
cd ${_PWD}
### WARNING: DO NOT CHANGE CODES until HERE!!! ###

export VERSION=1.0

CFLAGS="${CFLAGS}"

if [ "$ARCH" == "arm" ]; then
	CFLAGS="${CFLAGS} -g -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\""
else
	exit 0
	CFLAGS="${CFLAGS}"
fi

if [ $1 ];
then
	run make $1 || exit 1
else
	run ./autogen.sh || exit 1

	if [ "$ARCH" == "arm" ];
	then
		BUILDOPTION=""
	else
		BUILDOPTION=""
	fi

	if [[ "x$MACHINE" == "xprotector" && "x$DISTRO" == "xvodafone" ]];
	then
		BUILDOPTION=""
	fi
	
	run ./configure --prefix=$PREFIX $BUILDOPTION || exit 1
	run make || exit 1
	run make install || exit 1
	run make_pkg.sh || exit 1
fi


