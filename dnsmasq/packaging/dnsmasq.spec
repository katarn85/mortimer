Name:       dnsmasq
Summary:    dnsmasq, DNS forwarder.
Version:    2.57_8
Release:    7
Group:      TO_BE/FILLED_IN
License:    TO BE FILLED IN
Source0:    %{name}-%{version}.tar.gz
BuildRequires: cmake
BuildRequires: pkgconfig(dbus-1)

%description
Dnsmasq is a lightweight, easy to configure DNS forwarder and DHCP server.

%prep
%setup -q

%build
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}
make %{?jobs:-j%jobs}

%post
mkdir -p /opt/var/lib/misc

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}/usr/share/license
cp COPYING %{buildroot}/usr/share/license/dnsmasq

%files
%manifest dnsmasq.manifest
%{_bindir}/dnsmasq
/usr/share/license/dnsmasq

%changelog
* Wed Jul 24 2013 Seungyoun Ju <sy39.ju@samsung.com> 2.57_8
- Patch for binary size reduction

* Thu Oct 11 2012 Seungyoun Ju <sy39.ju@samsung.com> 2.57_7
- License file is added to package

* Mon Sep 24 2012 Seungyoun Ju <sy39.ju@samsung.com> 2.57_6
- Send the indication for DHCP connection every time

* Fri Sep 21 2012 Seungyoun Ju <sy39.ju@samsung.com> 2.57-5
- Default manifest file is added

* Mon Apr 16 2012 Seungyoun Ju <sy39.ju@samsung.com> 2.57-4
- "/opt/var/lib/misc" directory for lease file is created explicitly
