Name:           iw
Summary:        iw network utility
License:        ISC
Version:        3.15+DA1
Release:        0
Url:            http://udhcp.busybox.net
Source0:        %{name}-%{version}.tar.gz
Source1001:     %{name}.manifest
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
BuildRequires: pkgconfig(libnl-2.0)

%description
iw network utility

%prep 
%setup -q

%build
cp %{SOURCE1001} .
%{__make}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/sbin

cp -f ./iw %{buildroot}/usr/sbin/iw

%post
chmod 755 /usr/sbin/iw

%files
/usr/sbin/iw
