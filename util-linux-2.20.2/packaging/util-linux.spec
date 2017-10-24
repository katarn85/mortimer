Name:           util-linux
Version:        2.20.2
Release:        4
License:        GPLv2 and GPLv2+ and BSD with advertising and Public Domain
Summary:        A collection of basic system utilities
Url:            http://www.kernel.org/pub/linux/utils/util-linux/
Group:          System/Base
Source0:        http://www.kernel.org/pub/linux/utils/util-linux/v2.20/util-linux-%{version}.tar.bz2
# Source8:        login.pamd
Source9:        remote.pamd
Source1001:     packaging/util-linux.manifest
Patch0: 	packaging/losetup-2.20.1.patch

BuildRequires:  pam-devel
BuildRequires:  popt-devel
BuildRequires:  zlib-devel
BuildRequires:  pkgconfig(ncurses)
Provides:       util-linux-ng = %{version}
Requires(post): /bin/chown

%description
The util-linux package contains a large variety of low-level system
utilities that are necessary for a Linux system to function. Among
others, Util-linux contains the fdisk configuration tool and the login
program.

%package -n libblkid
License:        LGPL-2.0+
Summary:        Block device ID library
Group:          System/Libraries

%description -n libblkid
This is block device identification library, part of util-linux.

%package -n libblkid-devel
License:        LGPL-2.0+
Summary:        Block device ID library
Group:          Development/Libraries
Requires:       libblkid = %{version}

%description -n libblkid-devel
This is the block device identification development library and headers,
part of util-linux.

%package -n libuuid
License:        BSD
Summary:        Universally unique ID library
Group:          System/Libraries

%description -n libuuid
This is the universally unique ID library, part of e2fsprogs.

The libuuid library generates and parses 128-bit universally unique
id's (UUID's).  A UUID is an identifier that is unique across both
space and time, with respect to the space of all UUIDs.  A UUID can
be used for multiple purposes, from tagging objects with an extremely
short lifetime, to reliably identifying very persistent objects
across a network.

See also the "uuid" package, which is a separate implementation.

%package -n libuuid-devel
License:        BSD
Summary:        Universally unique ID library
Group:          Development/Libraries
Requires:       libuuid = %{version}

%description -n libuuid-devel
This is the universally unique ID development library and headers,
part of e2fsprogs.

The libuuid library generates and parses 128-bit universally unique
id's (UUID's).  A UUID is an identifier that is unique across both
space and time, with respect to the space of all UUIDs.  A UUID can
be used for multiple purposes, from tagging objects with an extremely
short lifetime, to reliably identifying very persistent objects
across a network.

See also the "uuid-devel" package, which is a separate implementation.

%package -n uuidd
License:        GPL-2.0
Summary:        Helper daemon to guarantee uniqueness of time-based UUIDs
Group:          System/Daemons
Requires:       libuuid = %{version}

%description -n uuidd
The uuidd package contains a userspace daemon (uuidd) which guarantees
uniqueness of time-based UUID generation even at very high rates on
SMP systems.

%package -n libmount
License:        LGPL-2.0+
Summary:        Mount interface library
Group:          System/Libraries

%description -n libmount
Mount interface library.

%package -n libmount-devel
License:        LGPL-2.0+
Summary:        Development files for libmount
Group:          Development/Libraries
Requires:       libmount = %{version}

%description -n libmount-devel
Development files for libmount.

%prep
%setup -q
%patch0 -p1

%build
cp %{SOURCE1001} .
unset LINGUAS || :

export CFLAGS="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 %{optflags}"
export SUID_CFLAGS="-fpie"
export SUID_LDFLAGS="-pie"
%configure \
        --bindir=/bin \
        --sbindir=/sbin \
        --disable-wall \
        --enable-write \
        --with-fsprobe=builtin \
        --disable-makeinstall-chown \
        --enable-raw \
        --without-ncurses \
        --without-slang \
        --without-selinux \
        --disable-nls \
	--disable-mountpoint
#        --disable-login-utils


make %{?_smp_mflags}

%install
%make_install


# And a dirs uuidd needs that the makefiles don't create
install -d %{buildroot}%{_localstatedir}/run/uuidd
install -d %{buildroot}%{_localstatedir}/lib/libuuid
mkdir -p %{buildroot}/etc/pam.d

# install -m 644 %{SOURCE8} %{buildroot}/etc/pam.d/login
install -m 644 %{SOURCE9} %{buildroot}/etc/pam.d/remote


# remove libtool junk
rm -f %{buildroot}%{_prefix}/lib/libblkid.la

# deprecated commands
for I in /sbin/fsck.minix /sbin/fsck /sbin/mkfs.{bfs,minix} /sbin/sln \
	/usr/bin/chkdupexe %{_bindir}/line %{_bindir}/pg %{_bindir}/{chfn,chsh,newgrp} \
	/sbin/shutdown %{_bindir}/scriptreplay /usr/sbin/vipw /usr/sbin/vigr; do
	rm -f %{buildroot}$I
done

# deprecated man pages
for I in man1/chkdupexe.1 man1/line.1 man1/pg.1 man1/newgrp.1 \
	man8/fsck.minix.8 man8/fsck.8 man8/mkfs.minix.8 man8/mkfs.bfs.8 man1/scriptreplay.1 \
	man8/vipw.8 man8/vigr; do
	rm -rf %{buildroot}%{_mandir}/${I}*
done


ln -sf ../../bin/kill %{buildroot}%{_bindir}/kill



# create list of setarch(8) symlinks
find  %{buildroot}%{_bindir}/ -regextype posix-egrep -type l \
	-regex ".*(linux32|linux64|s390|s390x|i386|ppc|ppc64|ppc32|sparc|sparc64|sparc32|sparc32bash|mips|mips64|mips32|ia64|x86_64)$" \
	-printf "%{_bindir}/%f\n" >> %{name}.files

find  %{buildroot}%{_mandir}/man8 -regextype posix-egrep  \
	-regex ".*(linux32|linux64|s390|s390x|i386|ppc|ppc64|ppc32|sparc|sparc64|sparc32|sparc32bash|mips|mips64|mips32|ia64|x86_64)\.8.*" \
	-printf "%{_mandir}/man8/%f*\n" >> %{name}.files

rm -f %{buildroot}%{_infodir}/dir

mkdir -p $RPM_BUILD_ROOT%{_datadir}/license
cat COPYING > $RPM_BUILD_ROOT%{_datadir}/license/util-linux
cat COPYING > $RPM_BUILD_ROOT%{_datadir}/license/uuidd
cat COPYING > $RPM_BUILD_ROOT%{_datadir}/license/libblkid
cat COPYING > $RPM_BUILD_ROOT%{_datadir}/license/libuuid
cat COPYING > $RPM_BUILD_ROOT%{_datadir}/license/libmount

%post
# only for minimal buildroots without /var/log
[ -d /var/log ] || /bin/mkdir -p /var/log
/bin/touch /var/log/lastlog
/bin/chown root:root /var/log/lastlog
/bin/chmod 0644 /var/log/lastlog



%post -n libblkid
/sbin/ldconfig
[ -e /etc/blkid.tab ] && mv /etc/blkid.tab /etc/blkid/blkid.tab || :
[ -e /etc/blkid.tab.old ] && mv /etc/blkid.tab.old /etc/blkid/blkid.tab.old || :

%postun -n libblkid -p /sbin/ldconfig

%post -n libuuid -p /sbin/ldconfig

%postun -n libuuid -p /sbin/ldconfig

%docs_package

%files  -f util-linux.files
%defattr(-,root,root)
%{_datadir}/license/util-linux
%ghost %attr(0644,root,root)    %verify(not md5 size mtime)     %{_localstatedir}/log/lastlog
/etc/pam.d/remote
#/etc/pam.d/login
/bin/dmesg
/bin/findmnt
/bin/lsblk
%exclude %attr(4755,root,root)   /bin/mount
%exclude %attr(4755,root,root)   /bin/umount
/sbin/agetty
/sbin/blkid
/sbin/blockdev
%exclude /sbin/ctrlaltdel
/sbin/fdisk
/sbin/findfs
%exclude /sbin/fsck.cramfs
/sbin/hwclock
/sbin/losetup
/sbin/mkfs
%exclude /sbin/mkfs.cramfs
/sbin/mkswap
/sbin/pivot_root
%exclude /sbin/raw
%exclude /sbin/sfdisk
/sbin/swapoff
/sbin/swapon
/sbin/switch_root
/sbin/fsfreeze
/sbin/fstrim
/sbin/swaplabel
%exclude /sbin/wipefs
%{_bindir}/cal
%{_bindir}/chrt
%{_bindir}/col
%exclude %{_bindir}/colcrt
%{_bindir}/colrm
%{_bindir}/column
%exclude %{_bindir}/cytune
%{_bindir}/flock
%{_bindir}/getopt
%{_bindir}/hexdump
%{_bindir}/ionice
%{_bindir}/ipcmk
%{_bindir}/ipcrm
%{_bindir}/ipcs
%exclude %{_bindir}/isosize
%{_bindir}/kill
%{_bindir}/logger
%exclude %{_bindir}/look
%exclude %{_bindir}/lscpu
%{_bindir}/mcookie
%{_bindir}/namei
%{_bindir}/rename
%{_bindir}/renice
%{_bindir}/rev
%{_bindir}/script
%{_bindir}/setarch
%{_bindir}/setsid
%exclude %{_bindir}/tailf
%{_bindir}/taskset
%{_bindir}/uuidgen
%{_bindir}/whereis
%attr(2755,root,tty)  %{_bindir}/write
%{_bindir}/fallocate
%{_bindir}/unshare
%{_sbindir}/fdformat
%{_sbindir}/ldattach
%{_sbindir}/readprofile
%{_sbindir}/rtcwake
%{_sbindir}/tunelp
%{_sbindir}/addpart
%{_sbindir}/delpart
%{_sbindir}/partx
%{_datadir}/getopt/getopt-parse.bash
%exclude %{_datadir}/getopt/getopt-parse.tcsh
%manifest util-linux.manifest

%files -n uuidd
%defattr(-,root,root)
%{_datadir}/license/uuidd
#/etc/rc.d/init.d/uuidd
%attr(-, uuidd, uuidd) %{_sbindir}/uuidd
%manifest util-linux.manifest

%files -n libblkid
%defattr(-,root,root)
%{_datadir}/license/libblkid
/%{_libdir}/libblkid.so.*
%manifest util-linux.manifest

%files -n libblkid-devel
%defattr(-,root,root)
%{_libdir}/libblkid.so
%{_includedir}/blkid
%{_libdir}/pkgconfig/blkid.pc
%manifest util-linux.manifest

%files -n libuuid
%defattr(-,root,root)
%{_datadir}/license/libuuid
%{_libdir}/libuuid.so.*
%manifest util-linux.manifest

%files -n libuuid-devel
%defattr(-,root,root)
%{_libdir}/libuuid.so
%{_includedir}/uuid
%{_libdir}/pkgconfig/uuid.pc
%manifest util-linux.manifest

%files -n libmount
%defattr(-,root,root)
%{_datadir}/license/libmount
%{_libdir}/libmount.so.*
%manifest util-linux.manifest

%files -n libmount-devel
%defattr(-,root,root)
%{_includedir}/libmount/libmount.h
%{_libdir}/libmount.so
%{_libdir}/pkgconfig/mount.pc
%manifest util-linux.manifest
