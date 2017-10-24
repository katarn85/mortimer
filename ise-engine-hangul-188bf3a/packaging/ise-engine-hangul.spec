Name:       ise-engine-hangul
Summary:    Hangul Input Method Engine for ISF
Version:    0.4.0
Release:    4
Group:      System Environment/Libraries
License:    GPL
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  gettext-tools
BuildRequires:  pkgconfig(isf)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(libhangul)

%description
Hangul Input Method Engine enables isf to input Hangul (Korean)
characters from the keyboard using the plugin modules and the data files.


%prep
%setup -q

%build

./bootstrap
%configure --prefix=%{_prefix} --disable-static

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install
mkdir -p %{buildroot}%{_datadir}/license
install -m0644 %{_builddir}/%{buildsubdir}/COPYING %{buildroot}%{_datadir}/license/%{name}


%files
%defattr(-,root,root,-)
%{_datadir}/scim/icons/*
%{_datadir}/scim/hangul/symbol.txt
%{_datadir}/locale/*/LC_MESSAGES/*
%{_libdir}/scim-1.0/1.4.0/IMEngine/hangul.so
%{_datadir}/license/%{name}

