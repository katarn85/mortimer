%global _hardened_build 1

# We ship a .pc file but don't want to have a dep on pkg-config. We
# strip the automatically generated dep here and instead co-own the
# directory.
%global __requires_exclude pkg-config

%define WITH_BASH_COMPLETION 0
%define WITH_BACKLIGHT 1
%define WITH_COREDUMP 0
%define WITH_PAM 0
%define WITH_RANDOMSEED 0

%define WITH_LOGIND 0
%define WITH_TIMEDATED 0

%define WITH_COMPAT_LIBS 1

Name:           systemd
Url:            http://www.freedesktop.org/wiki/Software/systemd
Version:        210
Release:        1
License:        LGPLv2+ and MIT and GPLv2+
Summary:        A System and Service Manager
Source0:        http://www.freedesktop.org/software/systemd/%{name}-%{version}.tar.xz
Source1:        apply-conf.sh
Source11:       tizen-system-mobile.conf
Source12:       tizen-system-wearable.conf
Source13:       tizen-journald.conf
Source14:       tizen-bootchart.conf
Source1001:     systemd.manifest
# The base number of patch for systemd is 101.

# The base number of patch for udev is 201.

BuildRequires:  glib2-devel
BuildRequires:  kmod-devel >= 15
BuildRequires:  hwdata

BuildRequires:  libacl-devel
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(dbus-1) >= 1.4.0
BuildRequires:  libcap-devel
BuildRequires:  libblkid-devel >= 2.20
BuildRequires:  pkgconfig(libkmod) >= 5
BuildRequires:  pam-devel
BuildRequires:  dbus-devel
BuildRequires:  libacl-devel
BuildRequires:  glib2-devel
BuildRequires:  libblkid-devel
BuildRequires:  xz-devel
BuildRequires:  kmod-devel
BuildRequires:  libgcrypt-devel
BuildRequires:  libxslt
BuildRequires:  intltool >= 0.40.0
BuildRequires:  gperf

BuildRequires:  gawk
BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  libtool
BuildRequires:  smack-devel
Requires:       dbus
Requires:       %{name}-libs = %{version}-%{release}

Provides:       /bin/systemctl
Provides:       /sbin/shutdown
Provides:       syslog
Provides:       systemd-units = %{version}-%{release}
Provides:       udev = %{version}
Obsoletes:      udev < 183
Obsoletes:      system-setup-keyboard < 0.9
Provides:       system-setup-keyboard = 0.9
Obsoletes:      nss-myhostname < 0.4
Provides:       nss-myhostname = 0.4
Obsoletes:      systemd < 204-10
Obsoletes:      systemd-analyze < 198
Provides:       systemd-analyze = 198

%description
systemd is a system and service manager for Linux, compatible with
SysV and LSB init scripts. systemd provides aggressive parallelization
capabilities, uses socket and D-Bus activation for starting services,
offers on-demand starting of daemons, keeps track of processes using
Linux cgroups, supports snapshotting and restoring of the system
state, maintains mount and automount points and implements an
elaborate transactional dependency-based service control logic. It can
work as a drop-in replacement for sysvinit.

%package libs
Summary:        systemd libraries
License:        LGPLv2+ and MIT
Provides:       libudev
Obsoletes:      systemd < 185-4
Conflicts:      systemd < 185-4

%description libs
Libraries for systemd and udev, as well as the systemd PAM module.

%package devel
Summary:        Development headers for systemd
License:        LGPLv2+ and MIT
Requires:       %{name} = %{version}-%{release}
Provides:       pkgconfig(libudev)
Provides:       libudev-devel = %{version}
Obsoletes:      libudev-devel < 183

%description devel
Development headers and auxiliary files for developing applications for systemd.

%package -n libgudev1
Summary:        Libraries for adding libudev support to applications that use glib
License:        LGPLv2+
Requires:       %{name} = %{version}-%{release}

%description -n libgudev1
This package contains the libraries that make it easier to use libudev
functionality from applications that use glib.

%package -n libgudev1-devel
Summary:        Header files for adding libudev support to applications that use glib
Requires:       libgudev1 = %{version}-%{release}
License:        LGPLv2+

%description -n libgudev1-devel
This package contains the header and pkg-config files for developing
glib-based applications using libudev functionality.

%package journal-gateway
Summary:        Gateway for serving journal events over the network using HTTP
Requires:       %{name} = %{version}-%{release}
License:        LGPLv2+
Requires(pre):    /usr/bin/getent
Requires(post):   systemd
Requires(preun):  systemd
Requires(postun): systemd

%description journal-gateway
systemd-journal-gatewayd serves journal events over the network using HTTP.

%prep
%setup -q -n %{name}-%{version}

%build
cp %{SOURCE1001} .

export CFLAGS+=" -g -O0 -DCONFIG_TIZEN -DCONFIG_TIZEN_WIP"
autoreconf -fiv
%configure \
    --disable-static \
    --with-sysvinit-path= \
    --with-sysvrcnd-path= \
    --disable-gtk-doc-html \
    --disable-selinux \
    --disable-ima \
    --disable-tcpwrap \
    --enable-split-usr \
    --disable-nls \
    --disable-manpages \
    --disable-efi \
%if %{WITH_PAM}
    --with-pamlibdir="/%{_libdir}/security" \
%else
    --disable-pam \
%endif
    --with-firmware-path="/%{_libdir}/firmware" \
    --with-zshcompletiondir="" \
    --disable-hostnamed \
    --disable-machined \
    --disable-binfmt \
    --disable-vconsole \
    --disable-quotacheck \
    --disable-localed \
    --disable-polkit \
    --disable-myhostname \
    --without-python \
%if "%{?tizen_profile_name}" == "wearable"
    --disable-xz \
%endif
%if ! %{WITH_RANDOMSEED}
    --disable-randomseed \
%endif
%if ! %{WITH_COREDUMP}
    --disable-coredump \
%endif
%if ! %{WITH_BACKLIGHT}
    --disable-backlight \
%endif
%if ! %{WITH_LOGIND}
    --disable-logind \
%endif
%if ! %{WITH_TIMEDATED}
    --disable-timedated \
%endif
%if %{WITH_COMPAT_LIBS}
    --enable-compat-libs \
%endif

make %{?_smp_mflags}

%install
%make_install

rm -f %{buildroot}/usr/bin/kernel-install
rm -rf %{buildroot}/usr/lib/kernel
rm -f %{buildroot}/usr/bin/systemd-detect-virt
find %{buildroot} \( -name '*.a' -o -name '*.la' \) -delete

# udev links
mkdir -p %{buildroot}/%{_sbindir}
mkdir -p %{buildroot}/sbin
ln -sf ../bin/udevadm %{buildroot}%{_sbindir}/udevadm

# Create SysV compatibility symlinks. systemctl/systemd are smart
# enough to detect in which way they are called.
ln -s ../lib/systemd/systemd %{buildroot}%{_sbindir}/init
ln -s /usr/lib/systemd/systemd %{buildroot}/sbin/init
ln -s ../lib/systemd/systemd %{buildroot}%{_bindir}/systemd
ln -s ../bin/systemctl %{buildroot}%{_sbindir}/reboot
ln -s /usr/bin/systemctl %{buildroot}/sbin/reboot
ln -s ../bin/systemctl %{buildroot}%{_sbindir}/halt
ln -s ../bin/systemctl %{buildroot}%{_sbindir}/poweroff
ln -s ../bin/systemctl %{buildroot}%{_sbindir}/shutdown
ln -s ../bin/systemctl %{buildroot}%{_sbindir}/telinit
ln -s ../bin/systemctl %{buildroot}%{_sbindir}/runlevel
ln -s /usr/lib/systemd/systemd-udevd %{buildroot}/%{_sbindir}/udevd
ln -s /lib/firmware %{buildroot}%{_libdir}/firmware

# We create all wants links manually at installation time to make sure
# they are not owned and hence overriden by rpm after the user deleted
# them.
rm -r %{buildroot}%{_sysconfdir}/systemd/system/*.target.wants

# Make sure these directories are properly owned
mkdir -p %{buildroot}%{_prefix}/lib/systemd/system/basic.target.wants
mkdir -p %{buildroot}%{_prefix}/lib/systemd/system/default.target.wants
mkdir -p %{buildroot}%{_prefix}/lib/systemd/system/dbus.target.wants

# Make sure the user generators dir exists too
mkdir -p %{buildroot}%{_prefix}/lib/systemd/system-generators
mkdir -p %{buildroot}%{_prefix}/lib/systemd/user-generators

# Create new-style configuration files so that we can ghost-own them
touch %{buildroot}%{_sysconfdir}/hostname
touch %{buildroot}%{_sysconfdir}/locale.conf
touch %{buildroot}%{_sysconfdir}/machine-id
touch %{buildroot}%{_sysconfdir}/machine-info
touch %{buildroot}%{_sysconfdir}/localtime
mkdir -p %{buildroot}%{_sysconfdir}/X11/xorg.conf.d
touch %{buildroot}%{_sysconfdir}/X11/xorg.conf.d/00-keyboard.conf

# Make sure the shutdown/sleep drop-in dirs exist
mkdir -p %{buildroot}%{_prefix}/lib/systemd/system-shutdown/
mkdir -p %{buildroot}%{_prefix}/lib/systemd/system-sleep/

# Make sure directories in /var exist
%if %{WITH_COREDUMP}
mkdir -p %{buildroot}%{_localstatedir}/lib/systemd/coredump
%endif
mkdir -p %{buildroot}%{_localstatedir}/lib/systemd/catalog
mkdir -p %{buildroot}%{_localstatedir}/log/journal
touch %{buildroot}%{_localstatedir}/lib/systemd/catalog/database
touch %{buildroot}%{_sysconfdir}/udev/hwdb.bin

# To avoid making life hard for Rawhide-using developers, don't package the
# kernel.core_pattern setting until systemd-coredump is a part of an actual
# systemd release and it's made clear how to get the core dumps out of the
# journal.
rm -f %{buildroot}%{_prefix}/lib/sysctl.d/50-coredump.conf

rm -rf %{buildroot}%{_prefix}/lib/udev/hwdb.d/

# These rules doesn't make much sense on Tizen right now.
rm -f %{buildroot}%{_libdir}/udev/rules.d/42-usb-hid-pm.rules
rm -f %{buildroot}%{_libdir}/udev/rules.d/60-persistent-storage-tape.rules
rm -f %{buildroot}%{_libdir}/udev/rules.d/60-cdrom_id.rules
rm -f %{buildroot}%{_libdir}/udev/rules.d/60-keyboard.rules
rm -f %{buildroot}%{_libdir}/udev/rules.d/61-accelerometer.rules
rm -f %{buildroot}%{_libdir}/udev/rules.d/64-btrfs.rules
rm -f %{buildroot}%{_libdir}/udev/rules.d/70-power-switch.rules
rm -f %{buildroot}%{_libdir}/udev/rules.d/75-net-description.rules
rm -f %{buildroot}%{_libdir}/udev/rules.d/75-probe_mtd.rules
rm -f %{buildroot}%{_libdir}/udev/rules.d/75-tty-description.rules
rm -f %{buildroot}%{_libdir}/udev/rules.d/80-net-name-slot.rules
rm -f %{buildroot}%{_libdir}/udev/rules.d/95-keyboard-force-release.rules
rm -f %{buildroot}%{_libdir}/udev/rules.d/95-keymap.rules

%if ! %{WITH_BASH_COMPLETION}
# Hmm, bash completion also removed :(
rm -f %{buildroot}%{_datadir}/bash-completion/completions/busctl
rm -f %{buildroot}%{_datadir}/bash-completion/completions/journalctl
rm -f %{buildroot}%{_datadir}/bash-completion/completions/kernel-install
%if %{WITH_LOGIND}
rm -f %{buildroot}%{_datadir}/bash-completion/completions/loginctl
%endif
%if %{WITH_TIMEDATED}
rm -f %{buildroot}%{_datadir}/bash-completion/completions/timedatectl
%endif
rm -f %{buildroot}%{_datadir}/bash-completion/completions/systemctl
%if %{WITH_COREDUMP}
rm -f %{buildroot}%{_datadir}/bash-completion/completions/systemd-coredumpctl
%endif
rm -f %{buildroot}%{_datadir}/bash-completion/completions/systemd-delta
rm -f %{buildroot}%{_datadir}/bash-completion/completions/systemd-run
rm -f %{buildroot}%{_datadir}/bash-completion/completions/udevadm
rm -f %{buildroot}%{_datadir}/bash-completion/completions/systemd-analyze
%endif

# This will be done by tizen specific init script.
rm -f %{buildroot}%{_libdir}/systemd/system/local-fs.target.wants/systemd-fsck-root.service

# We will just use systemd just as system, user not yet. Until that that service will be disabled.
%if ! %{WITH_LOGIND}
rm -f %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants/systemd-logind.service
%endif

# Make sure default extra dependencies ignore unit directory exist
mkdir -p %{buildroot}%{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d
%if %{WITH_TIMEDATED}
install -m644 %{_builddir}/%{name}-%{version}/src/timedate/org.freedesktop.timedate1.service %{buildroot}%{_prefix}/share/dbus-1/system-services/
%endif

# Apply Tizen configs
chmod +x %{SOURCE1}
%if "%{?tizen_profile_name}" == "mobile"
%{SOURCE1} %{SOURCE11} %{buildroot}%{_sysconfdir}/systemd/system.conf
%elseif "%{?tizen_profile_name}" == "wearable"
%{SOURCE1} %{SOURCE12} %{buildroot}%{_sysconfdir}/systemd/system.conf
%endif
%{SOURCE1} %{SOURCE13} %{buildroot}%{_sysconfdir}/systemd/journald.conf
%{SOURCE1} %{SOURCE14} %{buildroot}%{_sysconfdir}/systemd/bootchart.conf

# All of licenses should be manifested in /usr/share/license.
mkdir -p %{buildroot}%{_datadir}/license
cat LICENSE.LGPL2.1 > %{buildroot}%{_datadir}/license/systemd
cat LICENSE.LGPL2.1 > %{buildroot}%{_datadir}/license/systemd-libs
cat LICENSE.LGPL2.1 > %{buildroot}%{_datadir}/license/libgudev1


%remove_docs

%pre
getent group cdrom >/dev/null 2>&1 || groupadd -r -g 11 cdrom >/dev/null 2>&1 || :
getent group tape >/dev/null 2>&1 || groupadd -r -g 33 tape >/dev/null 2>&1 || :
getent group dialout >/dev/null 2>&1 || groupadd -r -g 18 dialout >/dev/null 2>&1 || :
getent group floppy >/dev/null 2>&1 || groupadd -r -g 19 floppy >/dev/null 2>&1 || :
getent group systemd-journal >/dev/null 2>&1 || groupadd -r -g 190 systemd-journal 2>&1 || :

systemctl stop systemd-udevd-control.socket systemd-udevd-kernel.socket systemd-udevd.service >/dev/null 2>&1 || :

%post
systemd-machine-id-setup >/dev/null 2>&1 || :
%if %{WITH_RANDOMSEED}
/usr/lib/systemd/systemd-random-seed save >/dev/null 2>&1 || :
%endif
systemctl daemon-reexec >/dev/null 2>&1 || :
systemctl start systemd-udevd.service >/dev/null 2>&1 || :
udevadm hwdb --update >/dev/null 2>&1 || :
journalctl --update-catalog >/dev/null 2>&1 || :

# Stop-gap until rsyslog.rpm does this on its own. (This is supposed
# to fail when the link already exists)
ln -s /usr/lib/systemd/system/rsyslog.service /etc/systemd/system/syslog.service >/dev/null 2>&1 || :
%if %{WITH_TIMEDATED}
ln -s ../system-services/org.freedesktop.timedate1.service %{_datadir}/dbus-1/services/org.freedesktop.timedate1.service
%endif
# Masked unnecessary units in tizen.
ln -s /dev/null /etc/systemd/system/systemd-tmpfiles-clean.timer
ln -s /dev/null /etc/systemd/system/systemd-remount-fs.service
ln -s /dev/null /etc/systemd/system/systemd-journal-flush.service
ln -s /dev/null /etc/systemd/system/systemd-update-utmp.service
ln -s /dev/null /etc/systemd/system/kmod-static-nodes.service

# Tizen is not supporting /usr/lib/macros.d yet.
# To avoid this, the macro will be copied to origin macro dir.
if [ -f %{_libdir}/rpm/macros.d/macros.systemd ]; then
        mkdir -p %{_sysconfdir}/rpm
        cp %{_libdir}/rpm/macros.d/macros.systemd %{_sysconfdir}/rpm
fi

%postun
if [ $1 -ge 1 ] ; then
        systemctl daemon-reload > /dev/null 2>&1 || :
%if %{WITH_LOGIND}
        systemctl try-restart systemd-logind.service >/dev/null 2>&1 || :
%endif
fi

%post libs -p /sbin/ldconfig
%postun libs -p /sbin/ldconfig

%post -n libgudev1 -p /sbin/ldconfig
%postun -n libgudev1 -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_datadir}/license/systemd
%dir %{_sysconfdir}/systemd
%dir %{_sysconfdir}/systemd/system
%dir %{_sysconfdir}/systemd/user
%dir %{_sysconfdir}/tmpfiles.d
%dir %{_sysconfdir}/sysctl.d
%dir %{_sysconfdir}/modules-load.d
%dir %{_sysconfdir}/udev
%dir %{_sysconfdir}/udev/rules.d
%dir %{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d
%dir %{_prefix}/lib/systemd
%dir %{_prefix}/lib/systemd/system-generators
%dir %{_prefix}/lib/systemd/user-generators
%dir %{_prefix}/lib/systemd/system-shutdown
%dir %{_prefix}/lib/systemd/system-sleep
%dir %{_prefix}/lib/systemd/catalog
%dir %{_prefix}/lib/tmpfiles.d
%dir %{_prefix}/lib/sysctl.d
%dir %{_prefix}/lib/modules-load.d
%{_prefix}/lib/firmware
%dir %{_datadir}/systemd
%dir %{_datadir}/pkgconfig
%dir %{_localstatedir}/log/journal
%dir %{_localstatedir}/lib/systemd
%dir %{_localstatedir}/lib/systemd/catalog
%if %{WITH_COREDUMP}
%dir %{_localstatedir}/lib/systemd/coredump
%endif
%if %{WITH_TIMEDATED}
%config(noreplace) %{_sysconfdir}/dbus-1/system.d/org.freedesktop.timedate1.conf
%endif
%config(noreplace) %{_sysconfdir}/dbus-1/system.d/org.freedesktop.systemd1.conf
%if %{WITH_LOGIND}
%config(noreplace) %{_sysconfdir}/dbus-1/system.d/org.freedesktop.login1.conf
%endif
%config(noreplace) %{_sysconfdir}/systemd/system.conf
%config(noreplace) %{_sysconfdir}/systemd/user.conf
%if %{WITH_LOGIND}
%config(noreplace) %{_sysconfdir}/systemd/logind.conf
%endif
%config(noreplace) %{_sysconfdir}/systemd/journald.conf
%config(noreplace) %{_sysconfdir}/systemd/bootchart.conf
%config(noreplace) %{_sysconfdir}/udev/udev.conf
%ghost %{_sysconfdir}/udev/hwdb.bin
%{_libdir}/rpm/macros.d/macros.systemd
%if %{WITH_PAM}
%config(noreplace) %{_sysconfdir}/pam.d/systemd-user
%endif
%{_sysconfdir}/xdg/systemd
%ghost %config(noreplace) %{_sysconfdir}/hostname
%ghost %config(noreplace) %{_sysconfdir}/localtime
%ghost %config(noreplace) %{_sysconfdir}/locale.conf
%ghost %config(noreplace) %{_sysconfdir}/machine-id
%ghost %config(noreplace) %{_sysconfdir}/machine-info
%ghost %config(noreplace) %{_sysconfdir}/X11/xorg.conf.d/00-keyboard.conf
%ghost %{_localstatedir}/lib/systemd/catalog/database
%{_bindir}/systemd
%{_bindir}/systemctl
%{_bindir}/systemd-run
%{_bindir}/systemd-notify
%{_bindir}/systemd-analyze
%{_bindir}/systemd-ask-password
%{_bindir}/systemd-tty-ask-password-agent
%{_bindir}/systemd-machine-id-setup
%{_bindir}/busctl
%if %{WITH_LOGIND}
%{_bindir}/loginctl
%endif
%if %{WITH_TIMEDATED}
%{_bindir}/timedatectl
%endif
%{_bindir}/journalctl
%{_bindir}/systemd-tmpfiles
%{_bindir}/systemd-nspawn
%{_bindir}/systemd-stdio-bridge
%{_bindir}/systemd-cat
%{_bindir}/systemd-cgls
%{_bindir}/systemd-cgtop
%{_bindir}/systemd-delta
%if %{WITH_LOGIND}
%{_bindir}/systemd-inhibit
%endif
%if %{WITH_COREDUMP}
%{_bindir}/systemd-coredumpctl
%endif
%{_bindir}/udevadm
%{_prefix}/lib/systemd/systemd
%{_prefix}/lib/systemd/system
%{_prefix}/lib/systemd/user
%{_prefix}/lib/systemd/systemd-*
%{_prefix}/lib/udev
%{_prefix}/lib/systemd/system-generators/systemd-getty-generator
%{_prefix}/lib/systemd/system-generators/systemd-fstab-generator
%exclude %{_prefix}/lib/systemd/system-generators/systemd-system-update-generator
%exclude %{_prefix}/lib/systemd/system-generators/systemd-gpt-auto-generator
%{_prefix}/lib/tmpfiles.d/systemd.conf
%{_prefix}/lib/tmpfiles.d/systemd-nologin.conf
%{_prefix}/lib/tmpfiles.d/x11.conf
%{_prefix}/lib/tmpfiles.d/tmp.conf
%{_prefix}/lib/sysctl.d/50-default.conf
%{_prefix}/lib/systemd/catalog/systemd.catalog
%{_prefix}/lib/systemd/catalog/systemd.fr.catalog
%{_prefix}/lib/systemd/catalog/systemd.it.catalog
%{_prefix}/lib/systemd/catalog/systemd.ru.catalog
%{_prefix}/lib/systemd/network/80-container-host0.network
%{_prefix}/lib/systemd/network/99-default.link
%{_sbindir}/init
/sbin/init
%{_sbindir}/reboot
/sbin/reboot
%{_sbindir}/halt
%{_sbindir}/poweroff
%{_sbindir}/shutdown
%{_sbindir}/telinit
%{_sbindir}/runlevel
%{_sbindir}/udevd
%{_sbindir}/udevadm
%{_datadir}/dbus-1/services/org.freedesktop.systemd1.service
%if %{WITH_TIMEDATED}
%{_datadir}/dbus-1/system-services/org.freedesktop.timedate1.service
%endif
%{_datadir}/dbus-1/system-services/org.freedesktop.systemd1.service
%if %{WITH_LOGIND}
%{_datadir}/dbus-1/system-services/org.freedesktop.login1.service
%endif
%if %{WITH_TIMEDATED}
%{_datadir}/dbus-1/interfaces/org.freedesktop.timedate1.xml
%endif
%{_datadir}/pkgconfig/systemd.pc
%{_datadir}/pkgconfig/udev.pc
%if %{WITH_BASH_COMPLETION}
%{_datadir}/bash-completion/completions/buxctl
%{_datadir}/bash-completion/completions/journalctl
%{_datadir}/bash-completion/completions/kernel-install
%if %{WITH_LOGIND}
%{_datadir}/bash-completion/completions/loginctl
%endif
%if %{WITH_TIMEDATED}
%{_datadir}/bash-completion/completions/timedatectl
%endif
%{_datadir}/bash-completion/completions/systemctl
%if %{WITH_COREDUMP}
%{_datadir}/bash-completion/completions/systemd-coredumpctl
%endif
%{_datadir}/bash-completion/completions/systemd-delta
%{_datadir}/bash-completion/completions/systemd-run
%{_datadir}/bash-completion/completions/udevadm
%{_datadir}/bash-completion/completions/systemd-analyze
%endif
%manifest systemd.manifest

%files libs
%defattr(-,root,root,-)
%{_datadir}/license/systemd-libs
%if %{WITH_PAM}
%{_libdir}/security/pam_systemd.so
%endif
%{_libdir}/libsystemd.so.*
%if %{WITH_COMPAT_LIBS}
%{_libdir}/libsystemd-daemon.so.*
%{_libdir}/libsystemd-journal.so.*
%{_libdir}/libsystemd-id128.so.*
%{_libdir}/libsystemd-login.so.*
%endif
%{_libdir}/libudev.so.*
%manifest systemd.manifest

%files devel
%defattr(-,root,root,-)
%dir %{_includedir}/systemd
%{_libdir}/libsystemd.so
%if %{WITH_COMPAT_LIBS}
%{_libdir}/libsystemd-daemon.so
%{_libdir}/libsystemd-journal.so
%{_libdir}/libsystemd-id128.so
%{_libdir}/libsystemd-login.so
%endif
%{_libdir}/libudev.so
%{_includedir}/systemd/_sd-common.h
%{_includedir}/systemd/sd-daemon.h
##%if %{WITH_LOGIND}
%{_includedir}/systemd/sd-login.h
##%endif
%{_includedir}/systemd/sd-journal.h
%{_includedir}/systemd/sd-id128.h
%{_includedir}/systemd/sd-messages.h
%{_includedir}/libudev.h
%{_libdir}/pkgconfig/libsystemd.pc
%if %{WITH_COMPAT_LIBS}
%{_libdir}/pkgconfig/libsystemd-daemon.pc
%{_libdir}/pkgconfig/libsystemd-journal.pc
%{_libdir}/pkgconfig/libsystemd-id128.pc
%{_libdir}/pkgconfig/libsystemd-login.pc
%endif
%{_libdir}/pkgconfig/libudev.pc
%manifest systemd.manifest

%files -n libgudev1
%defattr(-,root,root,-)
%{_datadir}/license/libgudev1
%{_libdir}/libgudev-1.0.so.*
%manifest systemd.manifest

%files -n libgudev1-devel
%defattr(-,root,root,-)
%{_libdir}/libgudev-1.0.so
%dir %{_includedir}/gudev-1.0
%dir %{_includedir}/gudev-1.0/gudev
%{_includedir}/gudev-1.0/gudev/*.h
%{_libdir}/pkgconfig/gudev-1.0*
%manifest systemd.manifest
