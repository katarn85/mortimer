Name:           kmod
Version:        15
Release:        1
Summary:        Linux kernel module management utilities

Group:          System Environment/Kernel
License:        GPLv2+
URL:            http://git.kernel.org/?p=utils/kernel/kmod/kmod.git;a=summary
Source0:        ftp://ftp.kernel.org/pub/linux/utils/kernel/kmod/%{name}-%{version}.tar.xz
Source1:        weak-modules
Source1001:     kmod.manifest
Exclusiveos:    Linux

BuildRoot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)
BuildRequires:  chrpath
BuildRequires:  zlib-devel
BuildRequires:  xz-devel
BuildRequires:  libxslt

Requires: %{name}-libs%{?_isa} = %{version}-%{release}

Provides:       module-init-tools = 4.0-1
Obsoletes:      module-init-tools < 4.0-1
Provides:       /sbin/modprobe

%description
The kmod package provides various programs needed for automatic
loading and unloading of modules under 2.6, 3.x, and later kernels, as well
as other module management programs. Device drivers and filesystems are two
examples of loaded and unloaded modules.

%package libs
Summary:        Libraries to handle kernel module loading and unloading
License:        LGPLv2+
Group:          System Environment/Libraries

%description libs
The kmod-libs package provides runtime libraries for any application that
wishes to load or unload Linux kernel modules from the running system.

%package devel
Summary:        Header files for kmod development
Group:          Development/Libraries
Requires:       %{name} = %{version}-%{release}

%description devel
The kmod-devel package provides header files used for development of
applications that wish to load or unload Linux kernel modules.

%prep
%setup -q

%build
cp %{SOURCE1001} .
export V=1
%configure \
        --with-zlib \
        --with-xz
make %{?_smp_mflags}

%install
make install DESTDIR=$RPM_BUILD_ROOT
pushd $RPM_BUILD_ROOT/%{_mandir}/man5
ln -s modprobe.d.5.gz modprobe.conf.5.gz
popd

rm -rf $RPM_BUILD_ROOT%{_libdir}/*.la
mkdir -p $RPM_BUILD_ROOT%{_sbindir}
ln -sf ../bin/kmod $RPM_BUILD_ROOT%{_sbindir}/modprobe
ln -sf ../bin/kmod $RPM_BUILD_ROOT%{_sbindir}/modinfo
ln -sf ../bin/kmod $RPM_BUILD_ROOT%{_sbindir}/insmod
ln -sf ../bin/kmod $RPM_BUILD_ROOT%{_sbindir}/rmmod
ln -sf ../bin/kmod $RPM_BUILD_ROOT%{_sbindir}/depmod
ln -sf ../bin/kmod $RPM_BUILD_ROOT%{_sbindir}/lsmod

mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/modprobe.d
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/depmod.d
mkdir -p $RPM_BUILD_ROOT%{_prefix}/lib/modprobe.d

mkdir -p $RPM_BUILD_ROOT/sbin
ln -sf  ..%{_sbindir}/depmod $RPM_BUILD_ROOT/sbin/depmod
install -m 755 %{SOURCE1} $RPM_BUILD_ROOT%{_sbindir}/weak-modules

mkdir -p $RPM_BUILD_ROOT%{_datadir}/license
cat COPYING > $RPM_BUILD_ROOT%{_datadir}/license/kmod
cat libkmod/COPYING > $RPM_BUILD_ROOT%{_datadir}/license/kmod-libs

%remove_docs

%post libs -p /sbin/ldconfig

%postun libs -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_datadir}/license/kmod
%dir %{_sysconfdir}/depmod.d
%dir %{_sysconfdir}/modprobe.d
%dir %{_prefix}/lib/modprobe.d
%{_bindir}/kmod
%{_sbindir}/modprobe
%{_sbindir}/modinfo
%{_sbindir}/insmod
%{_sbindir}/rmmod
%{_sbindir}/lsmod
%{_sbindir}/depmod
%{_sbindir}/weak-modules
%{_datadir}/bash-completion/completions/kmod
/sbin/depmod
%manifest kmod.manifest

%files libs
%defattr(-,root,root,-)
%{_datadir}/license/kmod-libs
%{_libdir}/libkmod.so.*
%manifest kmod.manifest

%files devel
%defattr(-,root,root,-)
%{_includedir}/libkmod.h
%{_libdir}/pkgconfig/libkmod.pc
%{_libdir}/libkmod.so
%manifest kmod.manifest
