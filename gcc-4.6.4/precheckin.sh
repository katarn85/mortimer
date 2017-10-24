#!/bin/bash

NAME=gcc
SPECNAME=${NAME}.spec
ARCHES="armv5tel armv6l armv7l armv7hl armv7nhl"
TOBASELIBS=""
TOBASELIBS_ARCH=""


# baselibs.conf - part 1
rm -f baselibs.conf baselibs.conf.old  2>&1 > /dev/null || true
echo -n "arch i586 targets " > baselibs.conf


for i in ${ARCHES} ; do
# cross spec files
    cat ./${SPECNAME} | sed -e "s#Name: .*#Name: cross-${i}-${NAME}#" > ./cross-${i}-${NAME}.spec
    cat ./${SPECNAME} | sed -e "s#Name: .*#Name: cross-${i}-${NAME}-accel-%{!?x64:x86}%{?x64}#" > ./cross-${i}-${NAME}-accel.spec
# baselibs.conf - part 2
    test ! x"$i" = x"" && echo -n "${i}:${i} " >> baselibs.conf
done

echo "" >> baselibs.conf
cat baselibs.conf | sed -e "s/i586/x86_64/" >> baselibs.conf

# baselibs.conf - part 3
echo "" >> baselibs.conf
for l in ${ARCHES} ; do
echo "" >> baselibs.conf
echo "cross-${l}-${NAME}-accel-@X86@
  targettype x86 block!
  targettype 32bit block!" >> baselibs.conf
for j in ${ARCHES//${l}} ; do
  echo "  targettype $j block!" >> baselibs.conf
done
cat >> baselibs.conf << EOF

  targettype ${l} autoreqprov off
  targettype ${l} provides "cross-arm-gcc-accel"
  targettype ${l} provides "cross-${l}-gcc-accel-${l}"
  targettype ${l} requires "eglibc-@X86@-arm"
  targettype ${l} requires "gmp-@X86@-arm"
  targettype ${l} requires "libgcc-@X86@-arm"
  targettype ${l} requires "mpfr-@X86@-arm"
  targettype ${l} requires "mpc-@X86@-arm"
  targettype ${l} requires "gcc"
  targettype ${l} requires "gcc-c++"

  targettype ${l} prefix /emul/ia32-linux
  targettype ${l} extension -arm
  targettype ${l} +/
  targettype ${l} -/usr/lib/debug
  targettype ${l} -/usr/lib64/debug
  targettype ${l} -/usr/src/debug
  targettype ${l} -/usr/share/man
  targettype ${l} -/usr/share/doc
  targettype ${l} -/usr/share/locale
  targettype ${l} requires "tizen-accelerator"
  targettype ${l} post "#set -x"
  targettype ${l} post " export GCCVER=\$(LANG=C gcc --version | head -1 | cut -d" " -f5) "
  targettype ${l} post " export GCCVER_NEW=\$(LANG=C <prefix>/usr/bin/gcc --version | head -1 | cut -d" " -f5) "
  targettype ${l} post " echo \"GCCVER: \$GCCVER     GCCVER_NEW: \$GCCVER_NEW \" "
  targettype ${l} post " if test "\${GCCVER}" == "\${GCCVER_NEW}"; then"
  targettype ${l} post "  echo "GCC and GCC-accel versions match. Enabling cross-compiler." "
  targettype ${l} post "  for bin in c++ cpp g++ gcc gcov; do "
  targettype ${l} post "   binary="/usr/bin/\${bin}" "
  targettype ${l} post "   if test -L \${binary} -a -e \${binary}.orig-arm ; then"
  targettype ${l} post "     echo "\${binary} not installed or \${binary}.orig-arm already present !" "
  targettype ${l} post "   else "
  targettype ${l} post "     mv \${binary} \${binary}.orig-arm && cp <prefix>\${binary} \${binary}"
  targettype ${l} post "   fi "
  targettype ${l} post "  done "

  targettype ${l} post "  for bin in cc1 cc1plus collect2 lto-wrapper lto1 ; do "
  targettype ${l} post "   binary="/usr/lib/gcc/${l}-tizen-linux-gnueabi/\$GCCVER/\$bin" "
  targettype ${l} post "   if test -L \${binary} -a -e \${binary}.orig-arm ; then"
  targettype ${l} post "     echo "\${binary} not installed or \${binary}.orig-arm already present !" "
  targettype ${l} post "   else "
  targettype ${l} post "     mv \${binary} \${binary}.orig-arm && cp <prefix>/usr/lib/gcc/${l}-tizen-linux-gnueabi/\$GCCVER_NEW/\${bin} \${binary}"
  targettype ${l} post "   fi "
  targettype ${l} post "  done "
  targettype ${l} post " else"
  targettype ${l} post "  echo "GCC and GCC-accel versions don't match. Rollback also binutils..." "
  targettype ${l} post "  for bin in addr2line ar as c++filt elfedit gprof ld ld.bfd ld.gold nm objcopy objdump ranlib readelf size strings strip ; do"
  targettype ${l} post "   binary="/usr/bin/\${bin}" "  
  targettype ${l} post "   if test -e \${binary}.orig-arm ; then"  
  targettype ${l} post "     rm \${binary} && mv \${binary}.orig-arm \${binary}"  
  targettype ${l} post "   else "  
  targettype ${l} post "     echo "\${binary}.orig-arm not present !" "  
  targettype ${l} post "   fi "  
  targettype ${l} post "  done "  
  targettype ${l} post "  rm -f /usr/lib/gcc/${l}-tizen-linux-gnueabi/\${GCCVER}/ld"
  targettype ${l} post "  rm -f /usr/lib/gcc/${l}-tizen-linux-gnueabi/\${GCCVER}/ld.bfd" 
  targettype ${l} post "  rm -f /usr/lib/gcc/${l}-tizen-linux-gnueabi/\${GCCVER}/ld.gold"
  targettype ${l} post " fi"

  targettype ${l} preun " export GCCVER=\$(LANG=C gcc --version | head -1 | cut -d" " -f5) "
  targettype ${l} preun " for i in c++ cpp g++ gcc gcov; do if test -e /usr/bin/\${i}.orig-arm ; then rm /usr/bin/\${i} ; mv /usr/bin/\${i}.orig-arm /usr/bin/\${i}; fi ; done "
  targettype ${l} preun " for i in cc1 cc1plus collect2 lto-wrapper lto1 ; do cd /usr/lib/gcc/${l}-tizen-linux-gnueabi/\$GCCVER ; if test -e \${i}.orig-arm ; then rm \${i} ; mv \${i}.orig-arm \${i} ; fi ; done "


EOF


done


exit 0

  targettype ${l} post " export GCCVER_MAJMIN=\$(echo "\$GCCVER" | cut -d"." -f1,2) "
  targettype ${l} post " export GCCVER_NEW_MAJMIN=\$(echo "\$GCCVER_NEW" | cut -d"." -f1,2) "
  targettype ${l} post " export CONTINUE=true "

  targettype ${l} post " if test -z "\$GCCVER" -o -z "\$GCCVER_NEW" -o -z "\$GCCVER_MAJMIN" -o -z "\$GCCVER_NEW_MAJMIN" ; then echo "ERROR: Can't determine all gcc versions! Not using cross-compiler!" ; exit 0 ; fi "
  targettype ${l} post " if test "\$GCCVER_MAJMIN" != "\$GCCVER_NEW_MAJMIN" ; then echo "GCC version differ in major or minor - not using cross-compiler!" ; export CONTINUE=false ; exit 0 ; fi "
