#sbs-git:slp/pkgs/a/alsa-utils alsa-utils 1.0.21 7b8a27d87cefc0e56d80383e61a0385e88cd90fd

Name:       alsa-utils
Summary:    Advanced Linux Sound Architecture (ALSA) utilities
Version:    1.0.24.4
Release:    4
Group:      Applications/Multimedia
License:    GPLv2+
URL:        http://www.alsa-project.org/
Source0:    ftp://ftp.alsa-project.org/pub/utils/alsa-utils-%{version}.tar.gz
BuildRequires: pkgconfig(alsa)


%description
This package contains command line utilities for the Advanced Linux Sound
Architecture (ALSA).



%package doc
Summary:    Man pages for alsa-utils
Group:      Documentation
Requires:   %{name} = %{version}-%{release}

%description doc
Man pages for alsa-utils



%prep
%setup -q -n %{name}-%{version}


%build

%configure --disable-static \
    --disable-nls \
    --disable-xmlto \
    --disable-alsamixer \
    --disable-alsatest \
    --with-udev-rules-dir=/usr/lib/udev/rules.d

make %{?jobs:-j%jobs}

exit

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp COPYING %{buildroot}/usr/share/license/%{name}

%make_install

%remove_docs

%files
%manifest alsa-utils.manifest
%{_bindir}/*
%{_sbindir}/*
%{_datadir}/alsa/*
%{_datadir}/sounds/*
/usr/lib/udev/rules.d/90-alsa-restore.rules
/usr/share/license/%{name}
