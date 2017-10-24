Name:		iptables
Summary:	Tools for managing Linux kernel packet filtering capabilities
Version:	1.4.21
Release:	1
Group:		System/Network
Source:		%{name}-%{version}.tar.gz
URL:		http://www.netfilter.org
License:	GNU General Public License version 2
BuildRequires:	kernel-headers

%description
The iptables utility controls the network packet filtering code in the
Linux kernel. If you need to set up firewalls and/or IP masquerading,
you should install this package.

%package devel
Summary:	Development package for iptables
Group:		System/Network
Requires:	%{name} = %{version}
Requires:	pkgconfig

%description devel
iptables development headers and libraries.

The iptc interface is upstream marked as not public. The interface is not
stable and may change with every new version. It is therefore unsupported.

%prep
%setup -q


%build
export CFLAGS+=" $RPM_OPT_FLAGS -Wall -Werror -O2 -D_FORTIFY_SOURCE=2 -fno-strict-aliasing"
export LDFLAGS+=" -Wl,--as-needed"

%configure --enable-devel --with-kernel=/usr --with-kbuild=/usr --with-ksource=/usr

# do not use rpath
sed -i 's|^hardcode_libdir_flag_spec=.*|hardcode_libdir_flag_spec=""|g' libtool
sed -i 's|^runpath_var=LD_RUN_PATH|runpath_var=DIE_RPATH_DIE|g' libtool

make %{?_smp_mflags}


%install
make install DESTDIR=%{buildroot}

# remove la file(s)
rm -f %{buildroot}/%{_libdir}/*.la

# install ip*tables.h header files
install -m 644 include/ip*tables.h %{buildroot}%{_includedir}/
install -d -m 755 %{buildroot}%{_includedir}/iptables
install -m 644 include/iptables/internal.h %{buildroot}%{_includedir}/iptables/

# install ipulog header file
install -d -m 755 %{buildroot}%{_includedir}/libipulog/
install -m 644 include/libipulog/*.h %{buildroot}%{_includedir}/libipulog/

# remove man pages
rm -rf %{buildroot}%{_mandir}

# License
mkdir -p %{buildroot}%{_datadir}/license
cp COPYING %{buildroot}%{_datadir}/license/iptables


%files
%manifest iptables.manifest
%{_sbindir}/iptables*
%{_sbindir}/ip6tables*
%{_sbindir}/xtables-multi
%{_bindir}/iptables-xml
%dir %{_libdir}/xtables
%{_libdir}/xtables/libipt*
%{_libdir}/xtables/libip6t*
%{_libdir}/xtables/libxt*
%{_libdir}/libip*tc.so.*
%{_libdir}/libxtables.so.*
%{_datadir}/license/iptables

%files devel
%dir %{_includedir}/iptables
%{_includedir}/iptables/*.h
%{_includedir}/*.h
%dir %{_includedir}/libiptc
%{_includedir}/libiptc/*.h
%dir %{_includedir}/libipulog
%{_includedir}/libipulog/*.h
%{_libdir}/libip*tc.so
%{_libdir}/libxtables.so
%{_libdir}/pkgconfig/libiptc.pc
%{_libdir}/pkgconfig/libip4tc.pc
%{_libdir}/pkgconfig/libip6tc.pc
%{_libdir}/pkgconfig/xtables.pc
