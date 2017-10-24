#sbs-git:slp/pkgs/f/freealut freealut 1.1.0 23c67d3e0917166b3ed62c65a8415bcac0380310
Name:       freealut
Summary:    OpenAL User Toolkit library
Version: 1.1.0
Release:    3
Group:      TO_BE/FILLED_IN
License:    TO BE FILLED IN
Source0:    %{name}-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(openal)


%description
OpenAL User Toolkit library development package



%package devel
Summary:    OpenAL User Toolkit library (devel)
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
OpenAL User Toolkit library development package (devel)


%prep
%setup -q 

%build
%configure --disable-static
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp COPYING %{buildroot}/usr/share/license/%{name}
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig





%files
%manifest freealut.manifest
%defattr(-,root,root,-)
%{_bindir}/freealut-config
%{_libdir}/libalut.so.*
/usr/share/license/%{name}


%files devel
%defattr(-,root,root,-)
%{_includedir}/AL/alut.h
%{_libdir}/libalut.so
%{_libdir}/pkgconfig/freealut.pc
