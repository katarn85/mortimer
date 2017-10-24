Name: iproute2
Summary: collection of utilities for controlling TCP/IP networking and traffic control in Linux
Version: 3.9.1
Release: 1
Source: %{name}-%{version}.tar.gz
Patch1: act_ipt_fix_xtables.patch
Patch2: gcc4_8_build_fix.patch
Group: System/Base
URL: https://www.kernel.org/pub/linux/utils/net/iproute2/
License: GPLv2
BuildRequires: kernel-headers
BuildRequires: bison
BuildRequires: flex
BuildRequires: db4-devel
BuildRequires: pkgconfig(xtables)
Conflicts: kernel < 2.4.20

%description
The iproute package contains networking utilities (ip, rtmon, tc etc)
which are designed to use the advanced networking capabilities of the Linux
2.4.x and 2.6.x kernel. Ip controls IPv4 and IPv6 configuration, tc stands
for traffic control. Both prints detailed usage. rtmon monitors the routing.
table changes.

%package devel
Summary: development files for iproutes libnetlink
Group: System/Base
Requires: %{name} = %{version}-%{release}

%description devel
Header files, library and documentation for libnetlink.
A library for accessing the netlink service.

%prep
%setup -q
%patch1 -p1
%patch2 -p1

%build
./configure
make %{?jobs:-j%jobs}

%install
mkdir -p \
    %{buildroot}%{_includedir} \
    %{buildroot}%{_sbindir} \
    %{buildroot}%{_mandir}/man3 \
    %{buildroot}%{_mandir}/man7 \
    %{buildroot}%{_mandir}/man8 \
    %{buildroot}%{_datadir}/tc \
    %{buildroot}%{_libdir}/tc \
    %{buildroot}/usr%{_sysconfdir}/iproute2 \
    %{buildroot}%{_datadir}/license

for binary in \
    bridge/bridge \
    genl/genl \
    ip/ifcfg \
    ip/ip \
    ip/routef \
    ip/routel \
    ip/rtmon \
    ip/rtpr \
    misc/arpd \
    misc/ifstat \
    misc/lnstat \
    misc/nstat \
    misc/rtacct \
    misc/ss \
    tc/tc
    do install -m755 ${binary} %{buildroot}%{_sbindir}
done

cd %{buildroot}%{_sbindir}
    ln -s lnstat ctstat
    ln -s lnstat rtstat
cd -

# Libs
install -m755 tc/m_xt.so %{buildroot}%{_libdir}/tc
cd %{buildroot}%{_libdir}/tc
    ln -s m_xt.so m_ipt.so
cd -

# libnetlink
install -m644 include/libnetlink.h %{buildroot}%{_includedir}
install -m644 lib/libnetlink.a %{buildroot}%{_libdir}

# Manpages
iconv -f latin1 -t utf8 man/man8/ss.8 > man/man8/ss.8.utf8 &&
    mv man/man8/ss.8.utf8 man/man8/ss.8
install -m644 man/man3/*.3 %{buildroot}%{_mandir}/man3
install -m644 man/man7/*.7 %{buildroot}%{_mandir}/man7
install -m644 man/man8/*.8 %{buildroot}%{_mandir}/man8

# Share files
for shared in \
    netem/normal.dist \
    netem/pareto.dist \
    netem/paretonormal.dist
    do install -m644 ${shared} %{buildroot}%{_datadir}/tc
done

# Config files
install -m644 etc/iproute2/* %{buildroot}/usr%{_sysconfdir}/iproute2

cp COPYING %{buildroot}%{_datadir}/license/iproute2
cp COPYING %{buildroot}%{_datadir}/license/iproute2-devel

%clean
rm -rf $RPM_BUILD_ROOT

%post
/sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%manifest iproute2.manifest
%dir /usr%{_sysconfdir}/iproute2
%attr(644,root,root) %config(noreplace) /usr%{_sysconfdir}/iproute2/*
%defattr(-,root,root)
%{_sbindir}/*
%dir %{_datadir}/tc
%{_datadir}/tc/*
%dir %{_libdir}/tc/
%{_libdir}/tc/*
%dir %{_datadir}/license/
%{_datadir}/license/iproute2

%files devel
%defattr(-,root,root)
%doc README README.decnet README.iproute2+tc README.distribution README.lnstat
%{_mandir}/man7/*
%{_mandir}/man8/*
%{_mandir}/man3/*
%{_libdir}/libnetlink.a
%{_includedir}/libnetlink.h
%{_datadir}/license/iproute2-devel
